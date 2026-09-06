// R11.6's BWEM fixture on the builder, with R11.9's corrections: BWEM's full analysis over
// synthetic terrain, with the minerals spaced so no two footprints touch, and the three
// teardown protocols that all survive on such a layout. The layout R11.6 used, with 2x1 mineral
// fields one tile apart, is what the builder refuses; the last case checks that it does.
#include "doctest.h"
#include "fixture.h"

#include <BWAPI/Client.h>
#include <bwem.h>

using namespace BWAPI;
using bwapi_c2::test::Fixture;
using bwapi_c2::test::FixtureError;

namespace {

constexpr int kMapW = 128, kMapH = 128;       // tiles, a standard ladder size
constexpr int kWalkW = kMapW * 4, kWalkH = kMapH * 4;

// R11.6's terrain: a walkable field, an unwalkable wall down the middle with one gap (the
// chokepoint), a raised plateau in each half, and an unwalkable border.
bool walkable(int wx, int wy) {
  const bool wall = (wx >= kWalkW / 2 - 6 && wx <= kWalkW / 2 + 6) &&
                    !(wy > kWalkH / 2 - 3 && wy < kWalkH / 2 + 3);
  const bool border = wx < 8 || wy < 8 || wx >= kWalkW - 8 || wy >= kWalkH - 8;
  return !(wall || border);
}

int height(int tx, int ty) {
  const bool plateau = (tx > 20 && tx < 45 && ty > 20 && ty < 45) ||
                       (tx > 83 && tx < 108 && ty > 83 && ty < 108);
  return plateau ? 2 : 0;
}

// Four mineral fields in each half, spaced one free tile apart (R11.9's --spaced layout): 2x1
// footprints at tiles 20, 22, 24, 26 never touch. BWEM places a base beside each cluster, at
// (22,7) and (104,105); the start locations sit on those tiles so that
// FindBasesForStartingLocations() has a base within its three-tile radius. R11.6's start
// locations at (16,16) and (108,108) were outside it, which is why its output said starting=0.
const TilePosition kStartA{22, 7}, kStartB{104, 105};

void build_synthetic_map(Fixture& f) {
  f.map(kMapW, kMapH, "Synthetic", "(2)Synthetic.scx");
  f.terrain(walkable, height);
  f.player(0, Races::Terran);
  f.player(1, Races::Zerg);
  f.start_location(kStartA.x, kStartA.y);
  f.start_location(kStartB.x, kStartB.y);
  for (int k = 0; k < 4; ++k) f.neutral(UnitTypes::Resource_Mineral_Field, 20 + 2 * k, 13);
  for (int k = 0; k < 4; ++k) f.neutral(UnitTypes::Resource_Mineral_Field, kMapW - 26 + 2 * k, kMapH - 17);
}

// BWEM's three-phase initialisation, in the order bwapi_bwem_initialize() collapses (plan 8.2).
BWEM::Map& analyse(Fixture& f) {
  BWEM::Map& m = BWEM::Map::Instance();
  m.Initialize(&f.game());
  m.EnableAutomaticPathAnalysis();
  REQUIRE(m.FindBasesForStartingLocations());
  return m;
}

}  // namespace

TEST_CASE("the real client sees the neutrals through the event stream") {
  Fixture f;
  build_synthetic_map(f);
  f.start();
  CHECK(Broodwar->getStartLocations().size() == 2);
  CHECK(Broodwar->getNeutralUnits().size() == 8);
  CHECK(Broodwar->getMinerals().size() == 8);
  CHECK(Broodwar->getStaticNeutralUnits().size() == 8);
  Unit u0 = Broodwar->getUnit(0);
  REQUIRE(u0 != nullptr);
  CHECK(u0->exists());
  CHECK(u0->getType() == UnitTypes::Resource_Mineral_Field);
  CHECK(u0->getPlayer()->isNeutral());
  CHECK(u0->getPlayer() == Broodwar->neutral());
  CHECK(u0->getResources() == 1500);
  CHECK(u0->getTilePosition() == TilePosition(20, 13));
}

TEST_CASE("BWEM's full analysis runs over synthetic terrain") {
  Fixture f;
  build_synthetic_map(f);
  f.start();
  BWEM::Map& m = analyse(f);

  CHECK(m.Initialized());
  CHECK(m.Size() == TilePosition(kMapW, kMapH));
  CHECK(m.Minerals().size() == 8);
  CHECK(m.Geysers().size() == 0);
  CHECK(m.MaxAltitude() > 0);
  CHECK(m.ChokePointCount() >= 1);
  CHECK(m.Areas().size() >= 2);
  REQUIRE(m.BaseCount() == 2);

  // The wall splits the map; the gap joins the halves.
  const BWEM::Area* left = m.GetArea(TilePosition(20, 64));
  const BWEM::Area* right = m.GetArea(TilePosition(108, 64));
  REQUIRE(left != nullptr);
  REQUIRE(right != nullptr);
  CHECK(left != right);
  CHECK(left->AccessibleFrom(right));

  int length = -1;
  const BWEM::CPPath& path = m.GetPath(Position(TilePosition(20, 64)), Position(TilePosition(108, 64)), &length);
  CHECK(path.size() >= 1);
  CHECK(length > 0);

  // One base per start location, each with its four minerals.
  int bases = 0;
  for (const BWEM::Area& a : m.Areas())
    for (const BWEM::Base& b : a.Bases()) {
      ++bases;
      CHECK(b.Starting());
      CHECK((b.Location() == kStartA || b.Location() == kStartB));
      CHECK(b.Minerals().size() == 4);
      CHECK(b.GetArea() == &a);
    }
  CHECK(bases == 2);
}

TEST_CASE("teardown protocols on an aligned layout (R11.9)") {
  SUBCASE("upstream's in-place re-Initialize survives") {
    Fixture f;
    build_synthetic_map(f);
    f.start();
    BWEM::Map& m = analyse(f);
    const int bases = m.BaseCount();
    m.Initialize(&f.game());
    m.EnableAutomaticPathAnalysis();
    CHECK(m.FindBasesForStartingLocations());
    CHECK(m.Initialized());
    CHECK(m.BaseCount() == bases);
    CHECK(m.Minerals().size() == 8);
  }
  SUBCASE("ResetInstance, then a fresh analysis") {
    Fixture f;
    build_synthetic_map(f);
    f.start();
    analyse(f);
    BWEM::Map::ResetInstance();
    CHECK_FALSE(BWEM::Map::Instance().Initialized());
    BWEM::Map& m2 = analyse(f);
    CHECK(m2.Initialized());
    CHECK(m2.BaseCount() == 2);
    CHECK(m2.Minerals().size() == 8);
    BWEM::Map::ResetInstance();
    BWEM::Map::ResetInstance();  // idempotent
    CHECK_FALSE(BWEM::Map::Instance().Initialized());
  }
  SUBCASE("the fixture tears the map down before the memory it points into") {
    {
      Fixture f;
      build_synthetic_map(f);
      f.start();
      analyse(f);
      CHECK(BWEM::Map::Instance().Initialized());
    }
    CHECK_FALSE(BWEM::Map::Instance().Initialized());
  }
}

TEST_CASE("the builder refuses R11.6's overlapping mineral layout") {
  Fixture f;
  f.neutral(UnitTypes::Resource_Mineral_Field, 20, 13);
  // A 2x1 field one tile to the right shares tile 21: the partial overlap BWEM accepts
  // silently in release and crashes on at teardown.
  CHECK_THROWS_AS(f.neutral(UnitTypes::Resource_Mineral_Field, 21, 13), FixtureError);
  // The same field on the same tiles is a deliberate stack only if the call says so.
  CHECK_THROWS_AS(f.neutral(UnitTypes::Resource_Mineral_Field, 20, 13), FixtureError);
  CHECK_NOTHROW(f.neutral(UnitTypes::Resource_Mineral_Field, 20, 13, 1500, true));
  // A different type on identical tiles is not an identical stack. Type_2 is 2x1 like the
  // field already there, so only the type comparison can be the reason this throws; a geyser
  // (4x2) would fail on geometry before the type is looked at.
  CHECK_THROWS_AS(f.neutral(UnitTypes::Resource_Mineral_Field_Type_2, 20, 13, 1500, true), FixtureError);
  // And a geyser (4x2) whose footprint reaches across other neutrals' tiles fails on geometry.
  CHECK_THROWS_AS(f.neutral(UnitTypes::Resource_Vespene_Geyser, 20, 13, 5000, true), FixtureError);
  // One free tile between fields is fine.
  CHECK_NOTHROW(f.neutral(UnitTypes::Resource_Mineral_Field, 23, 13));
}

TEST_CASE("the footprint rule") {
  using FP = Fixture::Footprint;
  const FP a{20, 13, 22, 14, UnitTypes::Resource_Mineral_Field};
  CHECK_FALSE(Fixture::partially_overlaps(a, FP{22, 13, 24, 14, UnitTypes::Resource_Mineral_Field}));  // adjacent
  CHECK(Fixture::partially_overlaps(a, FP{21, 13, 23, 14, UnitTypes::Resource_Mineral_Field}));        // shifted
  CHECK_FALSE(Fixture::partially_overlaps(a, a));                                                       // identical
  CHECK(Fixture::partially_overlaps(a, FP{20, 13, 22, 14, UnitTypes::Resource_Vespene_Geyser}));       // same tiles, other type
  CHECK(Fixture::partially_overlaps(a, FP{19, 12, 23, 14, UnitTypes::Resource_Vespene_Geyser}));       // contains
}
