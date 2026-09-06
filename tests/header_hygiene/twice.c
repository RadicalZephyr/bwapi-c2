/* All three public headers, each twice, as C99. Exercises the include guards and the macros
 * with the C preprocessor and type system. */
#include "bwapi_c2_types.h"
#include "bwapi_c2.h"
#include "bwapi_c2_bwem.h"
#include "bwapi_c2_types.h"
#include "bwapi_c2.h"
#include "bwapi_c2_bwem.h"

struct probe {
  int32_t size;
  int32_t a;
  int64_t b;
};

/* Every operand is a compile-time constant; the volatile copies keep these runtime checks
 * rather than constant conditions MSVC's /W4 rejects (C4127). */
int main(void) {
  volatile bwapi_position p = BWAPI_POS_MAKE(-7, 300000);
  volatile bwapi_position none = BWAPI_POSITION_NONE;
  volatile bwapi_position unknown_tile = BWAPI_TILEPOSITION_UNKNOWN;
  volatile int has_a = BWAPI_HAS_FIELD(struct probe, a, 8);
  volatile int has_b = BWAPI_HAS_FIELD(struct probe, b, 8);
  volatile int32_t none_id = BWAPI_NONE;
  if (BWAPI_POS_X(p) != -7 || BWAPI_POS_Y(p) != 300000) return 1;
  if (BWAPI_POS_X(none) != BWAPI_POSITION_NONE_X) return 2;
  if (BWAPI_POS_Y(unknown_tile) != BWAPI_TILEPOSITION_UNKNOWN_Y) return 3;
  if (!has_a) return 4;
  if (has_b) return 5;
  if (none_id != -1) return 6;
  return 0;
}
