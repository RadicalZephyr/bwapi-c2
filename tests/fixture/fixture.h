// The shared synthetic-GameData fixture (plan section 11; implementation plan 0.10).
//
// A Fixture owns a calloc'd BWAPI::GameData, fills in what a scenario asks for through the
// builder methods, and start() constructs the real client GameImpl over it and runs
// onMatchStart(). From then on Broodwar is the real API over synthetic memory: reads, BWAPI's
// own canXxx rule engine, command emission into data->unitCommands, and BWEM's full analysis,
// with no server, no shared memory, no StarCraft and no Blizzard data.
//
// The five invariants section 11 lists are encoded here, once, so no suite rediscovers them:
//   1. UnitImpl's constructor reads the global BWAPI::BWAPIClient.data, not the pointer handed
//      to GameImpl. start() sets the global before constructing anything.
//   2. Zero is a valid unit index; BWAPI's "none" is -1. Every unit-index field of a new unit
//      starts at -1.
//   3. isPowered and isInterruptible both gate canMove and both default false in memory. A unit
//      the scenario wants commandable gets them set.
//   4. Neutrals reach GameImpl through the UnitDiscover event stream, not a scan of
//      data->units, and PlayerImpl::isNeutral() reads a PlayerData flag. neutral() synthesises
//      the event and the fixture marks player 11 neutral.
//   5. Neutrals occupy their own tiles unless deliberately stacked with an identical footprint
//      and type. A partial overlap is a map-editor accident BWEM accepts silently and crashes
//      on at teardown (R11.9); neutral() refuses it by throwing FixtureError.
//
// Teardown order is the ABI's own (section 8.2): BWEM's map first, then GameImpl, then the
// memory it pointed into. The destructor does it so a test cannot get it wrong.
#pragma once

#include <BWAPI.h>
#include <BWAPI/Client/GameImpl.h>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace bwapi_c2::test {

// Thrown by the builder when a scenario violates an invariant. A test that wants to check the
// invariant is enforced asserts on this; nothing else in the suite should ever see one.
struct FixtureError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// Per-unit knobs beyond the required ones. Defaults describe a healthy, idle, commandable unit.
struct UnitOptions {
  int hit_points = -1;         // -1: the type's maxHitPoints()
  int shields = -1;            // -1: the type's maxShields()
  int energy = 0;
  bool completed = true;
  bool idle = true;
  bool powered = true;         // invariant 3
  bool interruptible = true;   // invariant 3
  bool visible_to_all = true;  // isVisible[p] for every player
  BWAPI::Order order = BWAPI::Orders::PlayerGuard;
};

class Fixture {
 public:
  Fixture();
  ~Fixture();
  Fixture(const Fixture&) = delete;
  Fixture& operator=(const Fixture&) = delete;

  // ---- building the world (before start()) --------------------------------------------

  // Map size in tiles, name and file name. Default 128x128 "Fixture", all walkable and buildable.
  Fixture& map(int width_tiles, int height_tiles, const char* name = "Fixture",
               const char* file_name = "(2)Fixture.scx");

  // Terrain from two functions: walkable per walk tile (4 per tile per axis), ground height per
  // tile (0..2). A tile is buildable when any of its sixteen walk tiles is walkable, which is
  // what R11.6's terrain did and what BWEM's analysis expects of buildable ground.
  using WalkableFn = std::function<bool(int wx, int wy)>;
  using HeightFn = std::function<int(int tx, int ty)>;
  Fixture& terrain(WalkableFn walkable, HeightFn height = nullptr);

  // A player. id 0 is self and 1 the enemy unless told otherwise; 11 is always the neutral
  // player and is set up by the constructor.
  Fixture& player(int id, BWAPI::Race race, int minerals = 50, int gas = 0,
                  const char* name = nullptr, bool participating = true);
  Fixture& self(int id);
  Fixture& enemy(int id);

  Fixture& start_location(int tx, int ty);

  // A unit at a pixel position. Returns its id (its index in data->units). Every index field
  // is -1 (invariant 2); powered and interruptible by default (invariant 3).
  int unit(int owner, BWAPI::UnitType type, int x, int y, const UnitOptions& opts = {});

  // A neutral (mineral field, geyser, static building) whose footprint's top-left tile is
  // (tx, ty); positioned at the footprint's centre exactly as the game does. Delivered through
  // a synthesised UnitDiscover event (invariant 4). Throws FixtureError on a footprint that
  // intersects an existing neutral's, unless `stacked` is true and the footprints and types
  // are identical (invariant 5).
  int neutral(BWAPI::UnitType type, int tx, int ty, int resources = 1500, bool stacked = false);

  // Queue an event for the next onMatchStart()/frame(). start() queues nothing itself: the
  // neutrals' UnitDiscover events are already there.
  Fixture& event(BWAPI::EventType::Enum type, int v1 = -1, int v2 = -1);

  // ---- running --------------------------------------------------------------------------

  // Invariant 1, then GameImpl, then BroodwarPtr, then onMatchStart(). data()->frameCount is
  // whatever the scenario set (default 0).
  void start();

  // One frame later: frameCount advances, the outgoing counters (unitCommands, shapes,
  // strings, commands) are cleared as the server would after consuming them, the queued
  // events are replaced by MatchFrame plus anything event() added since, and onMatchFrame()
  // pumps them. Multi-frame scenarios call this between frames.
  void frame();

  // The tests reach in when they need to: to flip a flag BWAPI reads (isPowered), to inspect
  // what a command wrote, or to hand the GameImpl to BWEM.
  BWAPI::GameData* data() { return data_; }
  BWAPI::GameImpl& game();
  bool started() const { return game_ != nullptr; }

  // The footprint rule, exposed so a test can check the arithmetic without a neutral.
  struct Footprint { int left, top, right, bottom; BWAPI::UnitType type; };  // right/bottom exclusive
  static bool partially_overlaps(const Footprint& a, const Footprint& b);

 private:
  int allocate_unit(int owner, BWAPI::UnitType type, int x, int y, const UnitOptions& opts);
  void require_not_started(const char* what) const;

  BWAPI::GameData* data_ = nullptr;
  std::unique_ptr<BWAPI::GameImpl> game_;
  std::vector<Footprint> neutral_footprints_;
  int unit_count_ = 0;
  int pending_consumed_ = 0;  // how many of data_->events the last pump saw
};

}  // namespace bwapi_c2::test
