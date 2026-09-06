// Handle resolution (plan section 6): range and kind, never existence. The range check is
// GameImpl's own: getUnit, getPlayer and getForce compare the id unsigned against their table
// (10,000 units, 12 players, 5 forces), so a negative id is out of range there too, and
// getRegion checks both ends against regionCount. Each returns null past the end; the latch is
// ours. A dead unit in range resolves to its UnitImpl and answers the way BWAPI answers for a
// dead unit, with no latch; a wrapper that added an exists() check here would break every bot
// that keeps ids across frames (section 6.2).
#include "abi_internal.h"

#include <BWAPI/Client/GameImpl.h>

namespace bwapi_c2 {

namespace {

std::string out_of_range(const char* fn, const char* kind, int32_t id) {
  return std::string(fn) + ": " + kind + " id " + std::to_string(id) + " could never have been valid";
}

}  // namespace

BWAPI::Unit resolve_unit(bwapi_unit_id id, const char* fn) {
  BWAPI::Unit u = BWAPI::BroodwarPtr->getUnit(id);
  if (!u) latch(BWAPI_ERR_INVALID_HANDLE, out_of_range(fn, "unit", id));
  return u;
}

BWAPI::Player resolve_player(bwapi_player_id id, const char* fn) {
  BWAPI::Player p = BWAPI::BroodwarPtr->getPlayer(id);
  if (!p) latch(BWAPI_ERR_INVALID_HANDLE, out_of_range(fn, "player", id));
  return p;
}

BWAPI::Force resolve_force(bwapi_force_id id, const char* fn) {
  BWAPI::Force f = BWAPI::BroodwarPtr->getForce(id);
  if (!f) latch(BWAPI_ERR_INVALID_HANDLE, out_of_range(fn, "force", id));
  return f;
}

BWAPI::Region resolve_region(bwapi_region_id id, const char* fn) {
  BWAPI::Region r = BWAPI::BroodwarPtr->getRegion(id);
  if (!r) latch(BWAPI_ERR_INVALID_HANDLE, out_of_range(fn, "region", id));
  return r;
}

}  // namespace bwapi_c2
