// Hand-written bulk data (plan section 9 names this file): in phase 1 the flat requiredUnits
// table of section 5.8, the one container-valued accessor that also ships as a table because
// every build-order planner walks it. The per-class row tables are emitted from spec/structs.yaml
// into src/bulk.gen.cpp; this file holds what a from: field cannot express. Phase 2 adds the map
// grids, collections, events and snapshots.
#include "abi_internal.h"

#include <utility>
#include <vector>

using namespace bwapi_c2;

extern "C" {

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
