// Hand-written bulk data (plan section 9 names this file): the flat requiredUnits table of
// section 5.8, the one container-valued accessor that also ships as a table because every
// build-order planner walks it, and from phase 2 the events of section 5.6. The per-class row
// tables are emitted from spec/structs.yaml into src/bulk.gen.cpp; this file holds what a
// from: field cannot express. The map grids, collections and snapshots follow in their steps.
#include "abi_internal.h"

#include <string>
#include <utility>
#include <vector>

using namespace bwapi_c2;

namespace {

// The event at index of the frame's snapshot, or null having latched. An index is checked the
// way a handle is (section 6.2): against the range it could be in, which for an event is the
// snapshot's size and nothing else.
const BWAPI::Event* event_at(int32_t index, const char* fn) {
  const auto& events = frame_events();
  if (index < 0 || static_cast<size_t>(index) >= events.size()) {
    latch(BWAPI_ERR_INVALID_HANDLE, std::string(fn) + ": event index " + std::to_string(index) +
                                        " is outside the " + std::to_string(events.size()) +
                                        " events of this frame");
    return nullptr;
  }
  return &events[static_cast<size_t>(index)];
}

}  // namespace

extern "C" {

// ---- events (section 5.6) ---------------------------------------------------------------------

BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_game_event_count(void) BWAPI_C2_NOEXCEPT {
  return guard<int32_t>(0, []() -> int32_t {
    if (!game_ready("bwapi_game_event_count")) return 0;
    return static_cast<int32_t>(frame_events().size());
  });
}

BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_game_get_event(int32_t index, bwapi_event* out) BWAPI_C2_NOEXCEPT {
  return guard<int32_t>(0, [&]() -> int32_t {
    if (!check_struct_out(out)) return 0;
    if (!game_ready("bwapi_game_get_event")) return 0;
    const BWAPI::Event* e = event_at(index, "bwapi_game_get_event");
    if (!e) return 0;
    write_struct(out, [&](bwapi_event& row) {
      row.type = static_cast<int32_t>(e->getType());
      // The accessors default to null and Positions::None for the types that do not carry
      // them, which id_of() and the raw x, y pass through as BWAPI_NONE and the pixel None.
      row.unit_id = id_of(e->getUnit());
      row.player_id = id_of(e->getPlayer());
      row.x = e->getPosition().x;
      row.y = e->getPosition().y;
      row.is_winner = e->isWinner() ? 1 : 0;
    });
    return 1;
  });
}

BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_game_event_text(int32_t index, char* buf, int32_t buf_len) BWAPI_C2_NOEXCEPT {
  return guard<int32_t>(0, [&]() -> int32_t {
    if (!check_string_buffer(buf, buf_len)) return 0;
    empty_string(buf, buf_len);
    if (!game_ready("bwapi_game_event_text")) return 0;
    const BWAPI::Event* e = event_at(index, "bwapi_game_event_text");
    if (!e) return 0;
    // getText() on an event without text is a shared empty string, not null (Event.cpp).
    return write_string(buf, buf_len, e->getText());
  });
}

// ---- the flat requiredUnits table (section 5.8) -------------------------------------------------

BWAPI_C2_API int32_t BWAPI_C2_CALL bwapi_unittype_required_units_table(bwapi_required_unit* out,
                                                                        int32_t cap) BWAPI_C2_NOEXCEPT {
  return guard<int32_t>(0, [&]() -> int32_t {
    if (!check_buffer(out, cap)) return 0;
    // Every (type, required type, count) triple, ascending by type and then by required type:
    // requiredUnits() is a std::map keyed by UnitType, so the inner order is already ascending.
    struct Triple { int32_t type, required, count; };
    std::vector<Triple> rows;
    const int32_t types = id_count<BWAPI::UnitType>();
    for (int32_t id = 0; id < types; ++id)
      for (const auto& req : BWAPI::UnitType(id).requiredUnits())
        rows.push_back({id, req.first.getID(), req.second});
    return write_rows(out, cap, static_cast<int32_t>(rows.size()), [&](bwapi_required_unit& row, int32_t i) {
      row.type = rows[static_cast<size_t>(i)].type;
      row.required_type = rows[static_cast<size_t>(i)].required;
      row.count = rows[static_cast<size_t>(i)].count;
    });
  });
}

}  // extern "C"
