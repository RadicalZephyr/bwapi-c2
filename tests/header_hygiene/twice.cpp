// All three public headers, each twice, as C++17. The declarations must be identical to what
// the C compiler saw, and BWAPI_C2_NOEXCEPT must spell noexcept here.
#include "bwapi_c2_types.h"
#include "bwapi_c2.h"
#include "bwapi_c2_bwem.h"
#include "bwapi_c2_types.h"
#include "bwapi_c2.h"
#include "bwapi_c2_bwem.h"

#include <type_traits>

static_assert(std::is_same<bwapi_position, int64_t>::value, "bwapi_position is int64_t");
static_assert(std::is_same<bwapi_unit_id, int32_t>::value, "handles are int32_t");
static_assert(noexcept(bwapi_last_error()), "every export is a noexcept boundary");
static_assert(BWAPI_POS_X(BWAPI_POS_MAKE(-7, 300000)) == -7, "packed x round-trips");
static_assert(BWAPI_POS_Y(BWAPI_POS_MAKE(-7, 300000)) == 300000, "packed y round-trips");
static_assert(BWAPI_POS_X(BWAPI_WALKPOSITION_INVALID) == 4000, "walk sentinel scale");

int main() { return 0; }
