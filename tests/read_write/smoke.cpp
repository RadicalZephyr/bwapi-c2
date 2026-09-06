// R7's harness on the fixture builder: the real client GameImpl over a synthetic GameData,
// reading, running BWAPI's own rule engine, and emitting a command into the outgoing buffer.
#include "doctest.h"
#include "fixture.h"

#include <BWAPI/Client.h>

using namespace BWAPI;
using bwapi_c2::test::Fixture;
using bwapi_c2::test::UnitOptions;

namespace {

// One Terran SCV owned by player 0 at (1000, 2000) on a 128x128 map, frame 123.
struct ScvScenario {
  Fixture f;
  int scv = -1;
  ScvScenario() {
    f.map(128, 128, "Fixture Map", "(2)Fixture.scx");
    f.player(0, Races::Terran, 350, 75, "FixtureBot");
    f.player(1, Races::Zerg, 50, 0, "Opponent");
    f.data()->frameCount = 123;
    f.data()->hasGUI = true;
    scv = f.unit(0, UnitTypes::Terran_SCV, 1000, 2000);
  }
};

}  // namespace

TEST_CASE("the real client reads the synthetic GameData") {
  ScvScenario s;
  s.f.start();
  CHECK(Broodwar->getClientVersion() == BWAPI::CLIENT_VERSION);
  CHECK(Broodwar->getFrameCount() == 123);
  CHECK(Broodwar->mapName() == "Fixture Map");
  CHECK(Broodwar->mapFileName() == "(2)Fixture.scx");
  CHECK(Broodwar->mapWidth() == 128);
  CHECK(Broodwar->mapHeight() == 128);
  CHECK(Broodwar->self()->getName() == "FixtureBot");
  CHECK(Broodwar->self()->minerals() == 350);
  CHECK(Broodwar->self()->gas() == 75);
  CHECK(Broodwar->self()->getRace() == Races::Terran);
  CHECK(Broodwar->enemy()->getRace() == Races::Zerg);
  CHECK(Broodwar->getAllUnits().size() == 1);
  // Invariant 4: the unit arrived by UnitDiscover, so the per-player sets know it too.
  CHECK(Broodwar->self()->getUnits().size() == 1);
  CHECK(Broodwar->self()->allUnitCount(UnitTypes::Terran_SCV) == 1);
  CHECK(Broodwar->self()->completedUnitCount(UnitTypes::Terran_SCV) == 1);
  CHECK(Broodwar->self()->allUnitCount(UnitTypes::Terran_Marine) == 0);
  CHECK(Broodwar->enemy()->getUnits().empty());

  Unit scv = Broodwar->getUnit(s.scv);
  REQUIRE(scv != nullptr);
  CHECK(scv->exists());
  CHECK(scv->getID() == s.scv);
  CHECK(scv->getType() == UnitTypes::Terran_SCV);
  CHECK(scv->getPosition() == Position(1000, 2000));
  CHECK(scv->getHitPoints() == UnitTypes::Terran_SCV.maxHitPoints());
  CHECK(scv->getType().isWorker());
  CHECK(scv->getType().mineralPrice() == 50);
  CHECK(scv->getPlayer() == Broodwar->self());
  // Invariant 2 in effect: an SCV with every index field at -1 is not loaded in unit 0.
  CHECK(scv->getTransport() == nullptr);
  CHECK(scv->isLoaded() == false);
}

TEST_CASE("BWAPI's own rule engine decides commandability") {
  ScvScenario s;
  s.f.start();
  Unit scv = Broodwar->getUnit(s.scv);
  REQUIRE(scv != nullptr);

  SUBCASE("a powered, interruptible SCV can move") {
    CHECK(scv->canCommand());
    CHECK(scv->canMove());
    CHECK(scv->canIssueCommandType(UnitCommandTypes::Move));
    CHECK(Broodwar->getLastError() == Errors::None);
  }
  SUBCASE("clearing isPowered fails canMove with BWAPI's real error code") {
    s.f.data()->units[s.scv].isPowered = false;
    CHECK_FALSE(scv->canMove());
    CHECK(Broodwar->getLastError() == Errors::Unit_Busy);
  }
  SUBCASE("a unit that does not exist cannot be commanded") {
    s.f.data()->units[s.scv].exists = false;
    CHECK_FALSE(scv->canCommand());
    CHECK(Broodwar->getLastError() == Errors::Unit_Does_Not_Exist);
  }
}

TEST_CASE("a command lands in the outgoing GameData buffer") {
  ScvScenario s;
  s.f.start();
  Unit scv = Broodwar->getUnit(s.scv);
  REQUIRE(scv != nullptr);
  REQUIRE(s.f.data()->unitCommandCount == 0);

  CHECK(scv->move(Position(1500, 2500)));
  CHECK(Broodwar->getLastError() == Errors::None);
  REQUIRE(s.f.data()->unitCommandCount == 1);
  const auto& c = s.f.data()->unitCommands[0];
  CHECK(UnitCommandType(c.type) == UnitCommandTypes::Move);
  CHECK(c.unitIndex == s.scv);
  CHECK(c.x == 1500);
  CHECK(c.y == 2500);

  Broodwar->drawTextScreen(10, 10, "hello fixture");
  CHECK(s.f.data()->shapeCount == 1);
  CHECK(s.f.data()->stringCount == 1);

  SUBCASE("the next frame starts with the buffers consumed") {
    s.f.frame();
    CHECK(Broodwar->getFrameCount() == 124);
    CHECK(s.f.data()->unitCommandCount == 0);
    CHECK(s.f.data()->shapeCount == 0);
    CHECK(s.f.data()->stringCount == 0);
    CHECK(Broodwar->getAllUnits().size() == 1);
    CHECK(scv->canMove());
  }
}

TEST_CASE("the builder stops at the unit index table") {
  // unitArray has 1700 slots against units' 10000; id is index here, so the 1701st unit has no
  // index and is refused rather than written over slot 0.
  Fixture f;
  f.player(0, Races::Terran);
  for (int i = 0; i < 1700; ++i) f.unit(0, UnitTypes::Terran_Marine, 100, 100);
  CHECK_THROWS_AS(f.unit(0, UnitTypes::Terran_Marine, 100, 100), bwapi_c2::test::FixtureError);
  CHECK(f.data()->unitArray[0] == 0);
  CHECK(f.data()->unitArray[1699] == 1699);
}

TEST_CASE("the fixture refuses to be built after start()") {
  ScvScenario s;
  s.f.start();
  CHECK_THROWS_AS(s.f.unit(0, UnitTypes::Terran_Marine, 0, 0), bwapi_c2::test::FixtureError);
  CHECK_THROWS_AS(s.f.map(64, 64), bwapi_c2::test::FixtureError);
}
