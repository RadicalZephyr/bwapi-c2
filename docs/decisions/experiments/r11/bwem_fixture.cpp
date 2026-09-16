// R11.5/R11.6: run BWEM's full analysis against a SYNTHETIC GameData.
// No StarCraft, no MPQs, no map file, no server. Terrain is generated here.
#include <BWAPI.h>
#include <BWAPI/Client/GameImpl.h>
#include <BWAPI/Client.h>
#include <bwem.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <chrono>
using namespace BWAPI;

static const int MAPW = 128, MAPH = 128;          // tiles  (a standard ladder size)
static const int WW = MAPW*4, WH = MAPH*4;        // walk tiles

int main(int argc, char** argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
  GameData* data = (GameData*)calloc(1, sizeof(GameData));
  data->client_version = BWAPI::CLIENT_VERSION;
  data->isInGame = true; data->hasGUI = false;
  data->mapWidth = MAPW; data->mapHeight = MAPH;
  strcpy(data->mapName, "Synthetic"); strcpy(data->mapFileName, "(2)Synthetic.scx");
  data->self = 0; data->enemy = 1; data->neutral = 11; data->playerCount = 12;

  // --- terrain: a walkable field, an unwalkable wall down the middle with one
  //     gap (a chokepoint), and a raised plateau in each half.
  for (int wy = 0; wy < WH; ++wy)
    for (int wx = 0; wx < WW; ++wx) {
      // a full-height wall with one narrow gap -> two areas joined by one chokepoint
      bool wall = (wx >= WW/2 - 6 && wx <= WW/2 + 6) && !(wy > WH/2 - 3 && wy < WH/2 + 3);
      bool border = wx < 8 || wy < 8 || wx >= WW-8 || wy >= WH-8;
      data->isWalkable[wx][wy] = !(wall || border);
    }
  for (int ty = 0; ty < MAPH; ++ty)
    for (int tx = 0; tx < MAPW; ++tx) {
      bool anyWalk = false;
      for (int i=0;i<4;++i) for (int j=0;j<4;++j) if (data->isWalkable[tx*4+i][ty*4+j]) anyWalk = true;
      data->isBuildable[tx][ty] = anyWalk;
      bool plateau = (tx > 20 && tx < 45 && ty > 20 && ty < 45) ||
                     (tx > 83 && tx < 108 && ty > 83 && ty < 108);
      data->getGroundHeight[tx][ty] = plateau ? 2 : 0;
    }
  data->startLocationCount = 2;
  data->startLocations[0].x = 16; data->startLocations[0].y = 16;
  data->startLocations[1].x = MAPW-20; data->startLocations[1].y = MAPH-20;

  // --- static neutrals: two mineral patches per start location, so
  //     FindBasesForStartingLocations() has something to build a Base from.
  int nUnits = 0;
  auto addMineral = [&](int tx, int ty) {
    int id = nUnits++;
    auto& u = data->units[id];
    u.exists = true; u.isCompleted = true; u.isVisible[0] = u.isVisible[1] = true;
    u.player = 11;                       // neutral
    u.type = UnitTypes::Resource_Mineral_Field;
    u.positionX = tx*32 + 32; u.positionY = ty*32 + 16;
    u.resources = 1500; u.isPowered = true;
    u.transport = u.target = u.orderTarget = u.buildUnit = -1;
    u.addon = u.nydusExit = u.powerUp = u.carrier = u.hatchery = -1;
    u.rallyUnit = -1; u.lastAttackerPlayer = -1;
    data->unitArray[id] = id;
  };
  for (int k = 0; k < 6; ++k) addMineral(20 + k, 13);
  for (int k = 0; k < 6; ++k) addMineral(MAPW - 26 + k, MAPH - 17);
  data->initialUnitCount = nUnits;
  data->players[11].type = PlayerTypes::Neutral;
  data->players[11].isNeutral = true;   // PlayerImpl::isNeutral() reads this flag, not the type
  data->players[11].race = Races::None;
  // GameImpl populates neutralUnits from the UnitDiscover EVENT STREAM, not from a
  // scan of data->units -- so a synthetic fixture must synthesise the events too.
  for (int i = 0; i < nUnits; ++i) {
    data->events[data->eventCount].type = EventType::UnitDiscover;
    data->events[data->eventCount].v1 = i;
    ++data->eventCount;
  }

  BWAPI::BWAPIClient.data = data;
  GameImpl game(data);
  BWAPI::BroodwarPtr = &game;
  game.onMatchStart();

  printf("  units=%d  events=%d  neutralUnits=%zu  minerals=%zu  staticNeutrals=%zu\n",
         data->initialUnitCount, data->eventCount,
         Broodwar->getNeutralUnits().size(), Broodwar->getMinerals().size(),
         Broodwar->getStaticNeutralUnits().size());
  { Unit u0 = Broodwar->getUnit(0);
    printf("  unit0: exists=%d type=%s player=%d isNeutral=%d isMineralField=%d\n",
      (int)(u0 && u0->exists()), u0?u0->getType().c_str():"?",
      u0&&u0->getPlayer()?u0->getPlayer()->getID():-1,
      (int)(u0 && u0->getPlayer() && u0->getPlayer()->isNeutral()),
      (int)(u0 && u0->getType().isMineralField())); }
  printf("synthetic map %dx%d tiles (%dx%d walk), %d start locations\n",
         MAPW, MAPH, WW, WH, (int)Broodwar->getStartLocations().size());

  using clk = std::chrono::steady_clock;
  auto t0 = clk::now();
  BWEM::Map& m = BWEM::Map::Instance();
  m.Initialize(&game);
  auto t1 = clk::now();
  m.EnableAutomaticPathAnalysis();
  bool ok = m.FindBasesForStartingLocations();
  auto t2 = clk::now();
  auto ms = [](auto a, auto b){ return std::chrono::duration<double,std::milli>(b-a).count(); };
  printf("Initialize()                    %8.1f ms\n", ms(t0,t1));
  printf("EnableAutoPath+FindBases()      %8.1f ms   (FindBases ok=%d)\n", ms(t1,t2), (int)ok);
  printf("TOTAL                           %8.1f ms\n", ms(t0,t2));

  printf("Initialized=%d  areas=%zu  chokepoints=%d  bases=%d  maxAltitude=%d\n",
         (int)m.Initialized(), m.Areas().size(), m.ChokePointCount(), m.BaseCount(),
         (int)m.MaxAltitude());
  for (const BWEM::Area& a : m.Areas())
    printf("  area %2d: group=%d miniTiles=%6d chokes=%zu bases=%zu top=(%d,%d)\n",
           (int)a.Id(), (int)a.GroupId(), a.MiniTiles(), a.ChokePoints().size(),
           a.Bases().size(), a.Top().x, a.Top().y);
  // a path query across the chokepoint
  int len = -1;
  const auto& path = m.GetPath(Position(TilePosition(20,64)), Position(TilePosition(108,64)), &len);
  printf("GetPath across the wall: %zu chokepoints, length=%d\n", path.size(), len);
  printf("minerals known to BWEM: %zu   geysers: %zu\n", m.Minerals().size(), m.Geysers().size());
  for (const BWEM::Area& a : m.Areas())
    for (const BWEM::Base& b : a.Bases())
      printf("  base in area %d at (%d,%d) starting=%d minerals=%zu\n",
             (int)a.Id(), b.Location().x, b.Location().y, (int)b.Starting(), b.Minerals().size());

  // Second Initialize on the same singleton -- the "consecutive games" scenario.
  // UPSTREAM BUG (R11.6): with neutrals present this segfaults in
  // Neutral::~Neutral -> RemoveFromTiles -> MapImpl::GetTile_, reached from
  // MapImpl::~MapImpl inside MapImpl::Initialize's in-place reset:
  //     this->~MapImpl(); new (this) MapImpl();
  // Stardust's vendored BWEM works around it with Map::ResetInstance(), which
  // BWEM-community does not have. Pass --reinit to reproduce.
  if (argc > 1 && !strcmp(argv[1], "--reinit")) {
    printf("re-Initialize with %zu neutrals (expect SIGSEGV upstream)...\n", m.Minerals().size());
    auto t3 = clk::now(); m.Initialize(&game); auto t4 = clk::now();
    printf("re-Initialize %7.1f ms  areas=%zu\n", ms(t3,t4), m.Areas().size());
  }
  // BWEM's singleton is destroyed by static destruction, AFTER main returns and
  // after this free() -- and ~Neutral reaches back into the Map's tiles. Both
  // orderings crash. See R11.6; the wrapper must tear BWEM down explicitly.
  if (argc > 1 && !strcmp(argv[1], "--exit-clean")) { printf("clean exit\n"); _exit(0); }
  free(data);
  return 0;
}
