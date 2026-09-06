// R11.9, as an executable check that BWEM's assertions are live in this configuration.
//
// Two 2x1 mineral fields one tile apart are planted behind the builder's back (Fixture::neutral
// would refuse them) and handed to BWEM. In release BWEM accepts the misaligned stack silently
// and the process dies at teardown, far from the cause; with BWAPI_C2_BWEM_ASSERTS it aborts
// inside Neutral::PutOnTiles(), naming the assertion. CTest registers this only in the asserts
// configuration and passes it when that assertion's name appears in the output.
#include "fixture.h"

#include <BWAPI/Client.h>
#include <bwem.h>

#include <cstdio>

using namespace BWAPI;

int main() {
  bwapi_c2::test::Fixture f;
  f.player(0, Races::Terran);
  f.player(1, Races::Zerg);
  f.start_location(22, 7);
  f.start_location(104, 105);
  for (int k = 0; k < 4; ++k) {
    // Tiles 20, 21, 22, 23 with 2x1 footprints: each overlaps the next by one tile.
    // unit() delivers the UnitDiscover itself; only the footprint check is being bypassed here.
    const int id = f.unit(11, UnitTypes::Resource_Mineral_Field, (20 + k) * 32 + 32, 13 * 32 + 16);
    f.data()->units[id].resources = 1500;
  }
  f.start();
  BWEM::Map::Instance().Initialize(&f.game());
  std::printf("overlap_asserts: Initialize returned with %zu minerals; BWEM's assertions are not live\n",
              BWEM::Map::Instance().Minerals().size());
  return 1;
}
