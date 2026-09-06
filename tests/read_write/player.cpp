// Every Player export against the fixture (implementation plan 1.4): the generated wrappers of
// spec/player.yaml over the real client GameImpl on a synthetic GameData. This is the first
// suite that goes through the C boundary rather than the C++ interface, so it also proves the
// generated prologue: a bad player id returns the neutral value and latches, a valid one does
// not touch the latch, and the string and collection conventions hold.
#include "doctest.h"
#include "fixture.h"

#include "bwapi_c2.h"

#include <cstring>
#include <string>

using namespace BWAPI;
using bwapi_c2::test::Fixture;

namespace {

// Three players the fixture always has: 0 (Terran, self), 1 (Zerg, enemy) and 11 (neutral). The
// scenario fills in what the builder leaves at zero: alliances, start locations, supply, the
// counters and levels the Player accessors read straight out of PlayerData.
struct PlayerScenario {
  Fixture f;
  int scv = -1, marine = -1, depot = -1, zergling = -1;
  PlayerScenario() {
    bwapi_clear_last_error();
    f.player(0, Races::Terran, 350, 75, "FixtureBot");
    f.player(1, Races::Zerg, 50, 0, "Opponent");
    auto* d = f.data();
    d->players[0].isEnemy[1] = true;
    d->players[1].isEnemy[0] = true;
    d->players[0].isAlly[0] = true;
    d->players[0].force = 1;
    d->players[1].force = 2;
    d->players[0].startLocationX = 7;
    d->players[0].startLocationY = 118;
    d->players[1].startLocationX = 120;
    d->players[1].startLocationY = 9;
    d->players[0].supplyTotal[Races::Terran] = 20;   // 10 as displayed
    d->players[0].supplyUsed[Races::Terran] = 6;     // 3 as displayed
    d->players[0].supplyTotal[Races::Zerg] = 2;
    d->players[0].gatheredMinerals = 1400;
    d->players[0].gatheredGas = 300;
    d->players[0].repairedMinerals = 12;
    d->players[0].repairedGas = 3;
    d->players[0].refundedMinerals = 75;
    d->players[0].refundedGas = 25;
    d->players[0].upgradeLevel[UpgradeTypes::Terran_Infantry_Armor] = 2;
    d->players[0].maxUpgradeLevel[UpgradeTypes::Terran_Infantry_Armor] = 3;
    d->players[0].maxUpgradeLevel[UpgradeTypes::U_238_Shells] = 1;
    d->players[0].hasResearched[TechTypes::Stim_Packs] = true;
    d->players[0].isResearching[TechTypes::Tank_Siege_Mode] = true;
    d->players[0].isUpgrading[UpgradeTypes::Terran_Infantry_Weapons] = true;
    d->players[0].isResearchAvailable[TechTypes::Stim_Packs] = true;
    d->players[0].isUnitAvailable[UnitTypes::Terran_Marine] = true;
    d->players[0].color = Colors::Red;
    d->players[1].color = Colors::Blue;
    d->players[0].totalUnitScore = 1234;
    d->players[0].totalKillScore = 500;
    d->players[0].totalBuildingScore = 800;
    d->players[0].totalRazingScore = 150;
    d->players[0].customScore = 42;
    d->players[0].deadUnitCount[UnitTypes::Terran_Marine] = 4;
    d->players[0].deadUnitCount[UnitTypes::AllUnits] = 5;
    d->players[0].killedUnitCount[UnitTypes::Zerg_Zergling] = 9;
    d->players[0].killedUnitCount[UnitTypes::AllUnits] = 9;
    d->players[1].leftGame = true;
    scv = f.unit(0, UnitTypes::Terran_SCV, 1000, 2000);
    marine = f.unit(0, UnitTypes::Terran_Marine, 1100, 2000);
    bwapi_c2::test::UnitOptions building;
    building.completed = false;
    depot = f.unit(0, UnitTypes::Terran_Supply_Depot, 1200, 2100, building);
    zergling = f.unit(1, UnitTypes::Zerg_Zergling, 3000, 3000);
    f.start();
  }
  ~PlayerScenario() { bwapi_clear_last_error(); }
};

std::string name_of(int32_t player) {
  char buf[64];
  const int32_t n = bwapi_player_get_name(player, buf, sizeof buf);
  return std::string(buf, static_cast<size_t>(n < 63 ? n : 63));
}

}  // namespace

TEST_CASE("identity: id, name, race, type, force, colour") {
  PlayerScenario s;
  CHECK(bwapi_player_get_id(0) == 0);
  CHECK(bwapi_player_get_id(11) == 11);
  CHECK(name_of(0) == "FixtureBot");
  CHECK(name_of(1) == "Opponent");
  CHECK(name_of(11) == "Neutral");
  CHECK(bwapi_player_get_race(0) == Races::Terran.getID());
  CHECK(bwapi_player_get_race(1) == Races::Zerg.getID());
  CHECK(bwapi_player_get_race(11) == Races::None.getID());
  CHECK(bwapi_player_get_type(0) == PlayerTypes::Player.getID());
  CHECK(bwapi_player_get_type(11) == PlayerTypes::Neutral.getID());
  CHECK(bwapi_player_get_force(0) == 1);
  CHECK(bwapi_player_get_force(1) == 2);
  CHECK(bwapi_player_get_color(0) == Colors::Red.getID());
  CHECK(bwapi_player_get_color(1) == Colors::Blue.getID());
  CHECK(bwapi_player_get_text_color(0) == Text::BrightRed);
  CHECK(bwapi_player_get_text_color(1) == Text::Blue);
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
}

TEST_CASE("the string convention on get_name") {
  PlayerScenario s;
  SUBCASE("a short buffer is truncated and NUL-terminated, and the full length comes back") {
    char buf[4];
    std::memset(buf, 'x', sizeof buf);
    CHECK(bwapi_player_get_name(0, buf, sizeof buf) == 10);
    CHECK(std::string(buf) == "Fix");
  }
  SUBCASE("NULL with zero length queries the length") {
    CHECK(bwapi_player_get_name(0, nullptr, 0) == 10);
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
  }
  SUBCASE("a bad buffer latches BAD_BUFFER before anything else, and nothing is written") {
    CHECK(bwapi_player_get_name(0, nullptr, 8) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
  }
  SUBCASE("a bad handle with a good buffer writes the empty string") {
    char buf[8];
    std::memset(buf, 'x', sizeof buf);
    CHECK(bwapi_player_get_name(12, buf, sizeof buf) == 0);
    CHECK(buf[0] == '\0');
    CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
  }
}

TEST_CASE("the collection convention on get_units") {
  PlayerScenario s;
  int32_t ids[8];
  SUBCASE("ascending ids, the total returned") {
    REQUIRE(bwapi_player_get_units(0, ids, 8) == 3);
    CHECK(ids[0] == s.scv);
    CHECK(ids[1] == s.marine);
    CHECK(ids[2] == s.depot);
    REQUIRE(bwapi_player_get_units(1, ids, 8) == 1);
    CHECK(ids[0] == s.zergling);
    CHECK(bwapi_player_get_units(11, ids, 8) == 0);
  }
  SUBCASE("a short buffer holds the first cap in id order and still reports the total") {
    ids[0] = ids[1] = -7;
    CHECK(bwapi_player_get_units(0, ids, 1) == 3);
    CHECK(ids[0] == s.scv);
    CHECK(ids[1] == -7);
  }
  SUBCASE("the size query") {
    CHECK(bwapi_player_get_units(0, nullptr, 0) == 3);
    CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
  }
  SUBCASE("a bad buffer latches") {
    CHECK(bwapi_player_get_units(0, nullptr, 4) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_BAD_BUFFER);
  }
}

TEST_CASE("alliances across the three players") {
  PlayerScenario s;
  CHECK(bwapi_player_is_ally(0, 0) == 1);
  CHECK(bwapi_player_is_ally(0, 1) == 0);
  CHECK(bwapi_player_is_enemy(0, 1) == 1);
  CHECK(bwapi_player_is_enemy(1, 0) == 1);
  CHECK(bwapi_player_is_enemy(0, 11) == 0);
  CHECK(bwapi_player_is_ally(0, 11) == 0);
  CHECK(bwapi_player_is_neutral(11) == 1);
  CHECK(bwapi_player_is_neutral(0) == 0);
  CHECK(bwapi_player_is_neutral(1) == 0);
  CHECK(bwapi_player_is_observer(0) == 0);
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);

  // The second handle is checked like the first: a bad `other` latches and returns 0.
  CHECK(bwapi_player_is_ally(0, 12) == 0);
  CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
  bwapi_clear_last_error();
  CHECK(bwapi_player_is_enemy(0, -1) == 0);
  CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
}

TEST_CASE("start location, game state, resources and supply") {
  PlayerScenario s;
  CHECK(bwapi_player_get_start_location(0) == BWAPI_POS_MAKE(7, 118));
  CHECK(BWAPI_POS_X(bwapi_player_get_start_location(1)) == 120);
  CHECK(BWAPI_POS_Y(bwapi_player_get_start_location(1)) == 9);
  CHECK(bwapi_player_is_victorious(0) == 0);
  CHECK(bwapi_player_is_defeated(0) == 0);
  CHECK(bwapi_player_left_game(0) == 0);
  CHECK(bwapi_player_left_game(1) == 1);

  CHECK(bwapi_player_minerals(0) == 350);
  CHECK(bwapi_player_gas(0) == 75);
  CHECK(bwapi_player_minerals(1) == 50);
  CHECK(bwapi_player_gathered_minerals(0) == 1400);
  CHECK(bwapi_player_gathered_gas(0) == 300);
  CHECK(bwapi_player_repaired_minerals(0) == 12);
  CHECK(bwapi_player_repaired_gas(0) == 3);
  CHECK(bwapi_player_refunded_minerals(0) == 75);
  CHECK(bwapi_player_refunded_gas(0) == 25);
  // spent = gathered - refunded - current, as PlayerImpl computes it.
  CHECK(bwapi_player_spent_minerals(0) == Broodwar->self()->spentMinerals());
  CHECK(bwapi_player_spent_gas(0) == Broodwar->self()->spentGas());

  // Supply is per race; BWAPI_RACE_NONE means the player's own race.
  CHECK(bwapi_player_supply_total(0, Races::None.getID()) == 20);
  CHECK(bwapi_player_supply_used(0, Races::None.getID()) == 6);
  CHECK(bwapi_player_supply_total(0, Races::Terran.getID()) == 20);
  CHECK(bwapi_player_supply_total(0, Races::Zerg.getID()) == 2);
  CHECK(bwapi_player_supply_used(0, Races::Protoss.getID()) == 0);
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
}

TEST_CASE("unit counts, with BWAPI_UNIT_ALL_UNITS spelled where C++ defaults it") {
  PlayerScenario s;
  const int32_t all = UnitTypes::AllUnits.getID();
  CHECK(bwapi_player_all_unit_count(0, all) == 3);
  CHECK(bwapi_player_all_unit_count(0, UnitTypes::Men.getID()) == 2);
  CHECK(bwapi_player_all_unit_count(0, UnitTypes::Buildings.getID()) == 1);
  CHECK(bwapi_player_all_unit_count(0, UnitTypes::Factories.getID()) == 0);
  CHECK(bwapi_player_all_unit_count(0, UnitTypes::Terran_SCV.getID()) == 1);
  CHECK(bwapi_player_all_unit_count(0, UnitTypes::Terran_Supply_Depot.getID()) == 1);
  CHECK(bwapi_player_completed_unit_count(0, all) == 2);
  CHECK(bwapi_player_completed_unit_count(0, UnitTypes::Terran_Supply_Depot.getID()) == 0);
  CHECK(bwapi_player_incomplete_unit_count(0, all) == 1);
  CHECK(bwapi_player_incomplete_unit_count(0, UnitTypes::Terran_Supply_Depot.getID()) == 1);
  CHECK(bwapi_player_all_unit_count(1, all) == 1);
  CHECK(bwapi_player_all_unit_count(1, UnitTypes::Zerg_Zergling.getID()) == 1);
  CHECK(bwapi_player_all_unit_count(11, all) == 0);
  CHECK(bwapi_player_visible_unit_count(0, all) == 3);
  CHECK(bwapi_player_visible_unit_count(0, UnitTypes::Terran_Marine.getID()) == 1);
  CHECK(bwapi_player_dead_unit_count(0, all) == 5);
  CHECK(bwapi_player_dead_unit_count(0, UnitTypes::Terran_Marine.getID()) == 4);
  CHECK(bwapi_player_killed_unit_count(0, all) == 9);
  CHECK(bwapi_player_killed_unit_count(0, UnitTypes::Zerg_Zergling.getID()) == 9);
  CHECK(bwapi_player_has_unit_type_requirement(0, UnitTypes::Terran_SCV.getID(), 1) == 1);
  CHECK(bwapi_player_has_unit_type_requirement(0, UnitTypes::Terran_SCV.getID(), 2) == 0);
  CHECK(bwapi_player_has_unit_type_requirement(0, UnitTypes::Terran_Supply_Depot.getID(), 1) == 0);
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
}

TEST_CASE("upgrades, research and availability") {
  PlayerScenario s;
  CHECK(bwapi_player_get_upgrade_level(0, UpgradeTypes::Terran_Infantry_Armor.getID()) == 2);
  CHECK(bwapi_player_get_upgrade_level(0, UpgradeTypes::Terran_Infantry_Weapons.getID()) == 0);
  CHECK(bwapi_player_get_max_upgrade_level(0, UpgradeTypes::Terran_Infantry_Armor.getID()) == 3);
  CHECK(bwapi_player_get_max_upgrade_level(0, UpgradeTypes::U_238_Shells.getID()) == 1);
  CHECK(bwapi_player_has_researched(0, TechTypes::Stim_Packs.getID()) == 1);
  CHECK(bwapi_player_has_researched(0, TechTypes::Tank_Siege_Mode.getID()) == 0);
  CHECK(bwapi_player_is_researching(0, TechTypes::Tank_Siege_Mode.getID()) == 1);
  CHECK(bwapi_player_is_researching(0, TechTypes::Stim_Packs.getID()) == 0);
  CHECK(bwapi_player_is_upgrading(0, UpgradeTypes::Terran_Infantry_Weapons.getID()) == 1);
  CHECK(bwapi_player_is_upgrading(0, UpgradeTypes::Terran_Infantry_Armor.getID()) == 0);
  CHECK(bwapi_player_is_research_available(0, TechTypes::Stim_Packs.getID()) == 1);
  CHECK(bwapi_player_is_research_available(0, TechTypes::Lockdown.getID()) == 0);
  CHECK(bwapi_player_is_unit_available(0, UnitTypes::Terran_Marine.getID()) == 1);
  CHECK(bwapi_player_is_unit_available(0, UnitTypes::Terran_Ghost.getID()) == 0);
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
}

TEST_CASE("the upgrade-aware unit type accessors agree with the C++ they wrap") {
  PlayerScenario s;
  Player p = Broodwar->self();
  const int32_t marine = UnitTypes::Terran_Marine.getID();
  const int32_t rifle = WeaponTypes::Gauss_Rifle.getID();
  CHECK(bwapi_player_max_energy(0, UnitTypes::Terran_Ghost.getID()) == p->maxEnergy(UnitTypes::Terran_Ghost));
  CHECK(bwapi_player_max_energy(0, UnitTypes::Terran_Ghost.getID()) == 200);
  CHECK(bwapi_player_top_speed(0, marine) == doctest::Approx(p->topSpeed(UnitTypes::Terran_Marine)));
  CHECK(bwapi_player_top_speed(0, marine) == doctest::Approx(UnitTypes::Terran_Marine.topSpeed()));
  CHECK(bwapi_player_weapon_max_range(0, rifle) == p->weaponMaxRange(WeaponTypes::Gauss_Rifle));
  CHECK(bwapi_player_weapon_max_range(0, rifle) == 128);
  CHECK(bwapi_player_sight_range(0, marine) == p->sightRange(UnitTypes::Terran_Marine));
  CHECK(bwapi_player_sight_range(0, marine) == 224);
  CHECK(bwapi_player_weapon_damage_cooldown(0, marine) == p->weaponDamageCooldown(UnitTypes::Terran_Marine));
  CHECK(bwapi_player_weapon_damage_cooldown(0, marine) == 15);
  // Two levels of infantry armour on top of the marine's 0.
  CHECK(bwapi_player_armor(0, marine) == p->armor(UnitTypes::Terran_Marine));
  CHECK(bwapi_player_armor(0, marine) == 2);
  CHECK(bwapi_player_damage(0, rifle) == p->damage(WeaponTypes::Gauss_Rifle));
  CHECK(bwapi_player_damage(0, rifle) == 6);
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
}

TEST_CASE("scores") {
  PlayerScenario s;
  CHECK(bwapi_player_get_unit_score(0) == 1234);
  CHECK(bwapi_player_get_kill_score(0) == 500);
  CHECK(bwapi_player_get_building_score(0) == 800);
  CHECK(bwapi_player_get_razing_score(0) == 150);
  CHECK(bwapi_player_get_custom_score(0) == 42);
  CHECK(bwapi_player_get_unit_score(1) == 0);
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
}

TEST_CASE("a bad player id returns the neutral value of every kind and latches once") {
  PlayerScenario s;
  for (int32_t bad : {-1, 12, 1000}) {
    bwapi_clear_last_error();
    CHECK(bwapi_player_minerals(bad) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
    char msg[128];
    bwapi_last_error_message(msg, sizeof msg);
    CHECK(std::string(msg).find("bwapi_player_minerals") != std::string::npos);
    // The latch is sticky: the next failures do not overwrite it and every kind is neutral.
    CHECK(bwapi_player_is_ally(bad, 0) == 0);
    CHECK(bwapi_player_get_race(bad) == Races::Unknown.getID());
    CHECK(bwapi_player_get_type(bad) == PlayerTypes::Unknown.getID());
    CHECK(bwapi_player_get_color(bad) == 255);
    CHECK(bwapi_player_get_force(bad) == BWAPI_NONE);
    // The neutral of a tile-scale function is the tile-scale sentinel, not the pixel one.
    CHECK(bwapi_player_get_start_location(bad) == BWAPI_TILEPOSITION_NONE);
    CHECK(bwapi_player_top_speed(bad, UnitTypes::Terran_Marine.getID()) == 0.0);
    CHECK(bwapi_player_get_units(bad, nullptr, 0) == 0);
    CHECK(bwapi_player_get_name(bad, nullptr, 0) == 0);
    CHECK(bwapi_last_error() == BWAPI_ERR_INVALID_HANDLE);
    bwapi_last_error_message(msg, sizeof msg);
    CHECK(std::string(msg).find("bwapi_player_minerals") != std::string::npos);
  }
}

TEST_CASE("an empty slot in range is not an error") {
  PlayerScenario s;
  // Player 5 was never set up: BWAPI answers for it as it does for any player, no latch.
  CHECK(bwapi_player_minerals(5) == 0);
  CHECK(bwapi_player_get_race(5) == Races::Zerg.getID());  // race 0 in a zeroed PlayerData
  CHECK(name_of(5).empty());
  CHECK(bwapi_player_get_units(5, nullptr, 0) == 0);
  CHECK(bwapi_last_error() == BWAPI_ERR_NONE);
}

TEST_CASE("before the game exists every Player call is NOT_CONNECTED") {
  bwapi_clear_last_error();
  REQUIRE(BroodwarPtr == nullptr);
  CHECK(bwapi_player_minerals(0) == 0);
  CHECK(bwapi_last_error() == BWAPI_ERR_NOT_CONNECTED);
  bwapi_clear_last_error();
}
