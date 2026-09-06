#include "fixture.h"

#include <BWAPI/Client.h>
#include <bwem.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace bwapi_c2::test {

namespace {

constexpr int kNeutralPlayer = 11;

// snprintf rather than strncpy: always NUL-terminates, and MSVC does not deprecate it (C4996).
void copy_string(char* dst, size_t cap, const char* src) {
  std::snprintf(dst, cap, "%s", src);
}

}  // namespace

Fixture::Fixture() {
  data_ = static_cast<BWAPI::GameData*>(std::calloc(1, sizeof(BWAPI::GameData)));
  if (!data_) throw FixtureError("calloc(sizeof(GameData)) failed");

  // What Server::initializeSharedMemory sets and every scenario needs (R7).
  data_->client_version = BWAPI::CLIENT_VERSION;
  data_->revision = 0;
  data_->isDebug = false;
  data_->isInGame = true;
  data_->hasGUI = false;
  data_->hasLatCom = false;
  data_->self = 0;
  data_->enemy = 1;
  data_->neutral = kNeutralPlayer;
  data_->playerCount = 12;
  map(128, 128);

  // Invariant 4: PlayerImpl::isNeutral() reads this flag, not the player type.
  auto& n = data_->players[kNeutralPlayer];
  n.type = BWAPI::PlayerTypes::Neutral;
  n.isNeutral = true;
  n.race = BWAPI::Races::None;
  copy_string(n.name, sizeof n.name, "Neutral");
}

Fixture::~Fixture() {
  // The ABI's shutdown order (section 8.2): BWEM's map holds BWAPI::Unit pointers into the
  // GameImpl, so it goes first; then the GameImpl; then the memory both pointed into.
  if (BWEM::Map::Instance().Initialized()) BWEM::Map::ResetInstance();
  game_.reset();
  BWAPI::BroodwarPtr = nullptr;
  BWAPI::BWAPIClient.data = nullptr;
  std::free(data_);
}

void Fixture::require_not_started(const char* what) const {
  if (game_) throw FixtureError(std::string(what) + " must be called before start()");
}

Fixture& Fixture::map(int width_tiles, int height_tiles, const char* name, const char* file_name) {
  require_not_started("map()");
  data_->mapWidth = width_tiles;
  data_->mapHeight = height_tiles;
  copy_string(data_->mapName, sizeof data_->mapName, name);
  copy_string(data_->mapFileName, sizeof data_->mapFileName, file_name);
  copy_string(data_->mapPathName, sizeof data_->mapPathName, file_name);
  // All walkable, all buildable, flat, until terrain() says otherwise.
  terrain([](int, int) { return true; });
  return *this;
}

Fixture& Fixture::terrain(WalkableFn walkable, HeightFn height) {
  require_not_started("terrain()");
  const int ww = data_->mapWidth * 4, wh = data_->mapHeight * 4;
  for (int wx = 0; wx < 1024; ++wx)
    for (int wy = 0; wy < 1024; ++wy)
      data_->isWalkable[wx][wy] = wx < ww && wy < wh && walkable(wx, wy);
  for (int tx = 0; tx < 256; ++tx)
    for (int ty = 0; ty < 256; ++ty) {
      const bool on_map = tx < data_->mapWidth && ty < data_->mapHeight;
      bool any_walkable = false;
      if (on_map)
        for (int i = 0; i < 4 && !any_walkable; ++i)
          for (int j = 0; j < 4 && !any_walkable; ++j)
            any_walkable = data_->isWalkable[tx * 4 + i][ty * 4 + j];
      data_->isBuildable[tx][ty] = any_walkable;
      data_->getGroundHeight[tx][ty] = on_map && height ? height(tx, ty) : 0;
      data_->isVisible[tx][ty] = on_map;
      data_->isExplored[tx][ty] = on_map;
    }
  return *this;
}

Fixture& Fixture::player(int id, BWAPI::Race race, int minerals, int gas, const char* name,
                         bool participating) {
  require_not_started("player()");
  if (id < 0 || id >= 12 || id == kNeutralPlayer)
    throw FixtureError("player id must be 0..10; 11 is the neutral player");
  auto& p = data_->players[id];
  p.race = race;
  p.type = BWAPI::PlayerTypes::Player;
  p.minerals = minerals;
  p.gas = gas;
  p.isParticipating = participating;
  p.isNeutral = false;
  copy_string(p.name, sizeof p.name, name ? name : (std::string("Player ") + std::to_string(id)).c_str());
  return *this;
}

Fixture& Fixture::self(int id) { require_not_started("self()"); data_->self = id; return *this; }
Fixture& Fixture::enemy(int id) { require_not_started("enemy()"); data_->enemy = id; return *this; }

Fixture& Fixture::start_location(int tx, int ty) {
  require_not_started("start_location()");
  if (data_->startLocationCount >= 8) throw FixtureError("at most 8 start locations");
  auto& s = data_->startLocations[data_->startLocationCount++];
  s.x = tx;
  s.y = ty;
  return *this;
}

int Fixture::allocate_unit(int owner, BWAPI::UnitType type, int x, int y, const UnitOptions& opts) {
  // The builder identifies a unit's id with its index in unitArray, and that table has 1700
  // slots (Brood War's concurrent-unit limit) against units' 10000 (ids over a whole game). A
  // 1701st unit would have no index and indexToUnit() could never name it, so refuse it rather
  // than write its id somewhere it does not belong.
  constexpr int kIndexTableSize = sizeof(data_->unitArray) / sizeof(data_->unitArray[0]);
  if (unit_count_ >= kIndexTableSize)
    throw FixtureError("GameData::unitArray indexes at most " + std::to_string(kIndexTableSize) +
                       " units; the builder identifies id with index");
  const int id = unit_count_++;
  auto& u = data_->units[id];
  u.id = id;
  u.exists = true;
  u.player = owner;
  u.type = type;
  u.positionX = x;
  u.positionY = y;
  u.hitPoints = opts.hit_points >= 0 ? opts.hit_points : type.maxHitPoints();
  u.lastHitPoints = u.hitPoints;
  u.shields = opts.shields >= 0 ? opts.shields : type.maxShields();
  u.energy = opts.energy;
  u.isCompleted = opts.completed;
  u.isIdle = opts.idle;
  u.isPowered = opts.powered;              // invariant 3
  u.isInterruptible = opts.interruptible;  // invariant 3
  u.order = opts.order;
  u.secondaryOrder = BWAPI::Orders::Nothing;
  for (int p = 0; p < 9; ++p) u.isVisible[p] = opts.visible_to_all || p == owner;
  // Invariant 2: zero is a valid unit index, so every index field starts at BWAPI's -1.
  u.transport = u.target = u.orderTarget = u.buildUnit = -1;
  u.addon = u.nydusExit = u.powerUp = u.carrier = u.hatchery = -1;
  u.rallyUnit = -1;
  u.lastAttackerPlayer = -1;
  u.buildType = BWAPI::UnitTypes::None;
  u.tech = BWAPI::TechTypes::None;
  u.upgrade = BWAPI::UpgradeTypes::None;
  data_->unitArray[id] = id;
  data_->initialUnitCount = unit_count_;
  // The server keeps per-type tallies in PlayerData that nothing derives from the unit sets,
  // and beside each type it tallies the four class pseudo-types the same way
  // (BWAPI/Source/BWAPI/GameUnits.cpp): AllUnits always; Buildings or Men by isBuilding(); and
  // Factories for a building that canProduce() or producesLarva().
  auto& tally = data_->players[owner];
  auto count = [&](BWAPI::UnitType t) {
    ++tally.allUnitCount[t];
    ++tally.visibleUnitCount[t];
    if (opts.completed) ++tally.completedUnitCount[t];
  };
  count(type);
  count(BWAPI::UnitTypes::AllUnits);
  if (type.isBuilding()) {
    count(BWAPI::UnitTypes::Buildings);
    if (type.canProduce() || type.producesLarva()) count(BWAPI::UnitTypes::Factories);
  } else {
    count(BWAPI::UnitTypes::Men);
  }
  return id;
}

int Fixture::unit(int owner, BWAPI::UnitType type, int x, int y, const UnitOptions& opts) {
  require_not_started("unit()");
  if (owner < 0 || owner >= 12) throw FixtureError("unit owner must be a player id 0..11");
  const int id = allocate_unit(owner, type, x, y, opts);
  // What the server does for every unit alive at match start: Server::update() queues the
  // frame's UnitDiscover events beside MatchStart, and only that event path puts a unit into
  // its PlayerImpl::units. onMatchStart() alone fills accessibleUnits, so without the event
  // getAllUnits() would see the unit and self()->getUnits() would not.
  event(BWAPI::EventType::UnitDiscover, id);
  return id;
}

bool Fixture::partially_overlaps(const Footprint& a, const Footprint& b) {
  const bool intersects = a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
  if (!intersects) return false;
  const bool identical = a.left == b.left && a.top == b.top && a.right == b.right &&
                         a.bottom == b.bottom && a.type == b.type;
  return !identical;
}

int Fixture::neutral(BWAPI::UnitType type, int tx, int ty, int resources, bool stacked) {
  require_not_started("neutral()");
  const Footprint fp{tx, ty, tx + type.tileWidth(), ty + type.tileHeight(), type};
  // Invariant 5.
  for (const Footprint& other : neutral_footprints_) {
    const bool intersects = fp.left < other.right && other.left < fp.right &&
                            fp.top < other.bottom && other.top < fp.bottom;
    if (!intersects) continue;
    if (partially_overlaps(fp, other))
      throw FixtureError("neutral " + std::string(type.c_str()) + " at (" + std::to_string(tx) + "," +
                         std::to_string(ty) + ") partially overlaps an existing neutral's footprint; "
                         "BWEM would build a misaligned stack and crash at teardown (R11.9)");
    if (!stacked)
      throw FixtureError("neutral " + std::string(type.c_str()) + " at (" + std::to_string(tx) + "," +
                         std::to_string(ty) + ") sits on an existing neutral; pass stacked=true if that "
                         "is what the scenario means");
  }
  neutral_footprints_.push_back(fp);

  UnitOptions opts;
  opts.idle = false;
  opts.order = BWAPI::Orders::Nothing;
  const int x = tx * 32 + type.tileWidth() * 16;
  const int y = ty * 32 + type.tileHeight() * 16;
  const int id = allocate_unit(kNeutralPlayer, type, x, y, opts);
  data_->units[id].resources = resources;
  // Invariant 4: GameImpl learns of neutrals from the event stream.
  event(BWAPI::EventType::UnitDiscover, id);
  return id;
}

namespace {

bool carries_text(BWAPI::EventType::Enum type) {
  return type == BWAPI::EventType::SendText || type == BWAPI::EventType::ReceiveText ||
         type == BWAPI::EventType::SaveGame;
}

}  // namespace

Fixture& Fixture::event(BWAPI::EventType::Enum type, int v1, int v2) {
  if (carries_text(type))
    throw FixtureError("SendText, ReceiveText and SaveGame carry text; queue them with event(type, text, player)");
  return queue(type, v1, v2);
}

Fixture& Fixture::queue(BWAPI::EventType::Enum type, int v1, int v2) {
  if (data_->eventCount >= BWAPI::GameData::MAX_EVENTS) throw FixtureError("event buffer full");
  auto& e = data_->events[data_->eventCount++];
  e.type = type;
  e.v1 = v1;
  e.v2 = v2;
  return *this;
}

Fixture& Fixture::event(BWAPI::EventType::Enum type, const char* text, int player) {
  if (!carries_text(type)) throw FixtureError("only SendText, ReceiveText and SaveGame events carry text");
  if (data_->eventStringCount >= BWAPI::GameData::MAX_EVENT_STRINGS) throw FixtureError("event string table full");
  const int slot = data_->eventStringCount++;
  copy_string(data_->eventStrings[slot], sizeof data_->eventStrings[slot], text ? text : "");
  // makeEvent() reads the slot from v1, except for ReceiveText, where v1 is the player and v2
  // the slot (BWAPIClient/Source/GameImpl.cpp).
  return type == BWAPI::EventType::ReceiveText ? queue(type, player, slot) : queue(type, slot, -1);
}

void Fixture::start() {
  require_not_started("start()");
  BWAPI::BWAPIClient.data = data_;           // invariant 1, before any UnitImpl exists
  game_ = std::make_unique<BWAPI::GameImpl>(data_);
  BWAPI::BroodwarPtr = game_.get();
  game_->onMatchStart();
  pending_consumed_ = data_->eventCount;  // onMatchStart pumped everything queued so far
}

BWAPI::GameImpl& Fixture::game() {
  if (!game_) throw FixtureError("game() before start()");
  return *game_;
}

void Fixture::frame() {
  if (!game_) throw FixtureError("frame() before start()");
  ++data_->frameCount;
  ++data_->elapsedTime;
  // The server consumed last frame's output.
  data_->unitCommandCount = 0;
  data_->shapeCount = 0;
  data_->stringCount = 0;
  data_->commandCount = 0;
  // Events queued since the last frame (by event()) stay; last frame's are gone. A MatchFrame
  // event leads, as the server sends it. eventStrings is left alone: a kept event may point at
  // a slot, and the 1000 slots outlast any scenario.
  const int queued = data_->eventCount;
  std::vector<BWAPIC::Event> keep(data_->events + pending_consumed_, data_->events + queued);
  data_->eventCount = 0;
  queue(BWAPI::EventType::MatchFrame, -1, -1);
  for (const auto& e : keep) queue(e.type, e.v1, e.v2);
  game_->onMatchFrame();
  pending_consumed_ = data_->eventCount;
}

}  // namespace bwapi_c2::test
