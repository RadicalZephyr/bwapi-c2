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

int main(void) {
  bwapi_position p = BWAPI_POS_MAKE(-7, 300000);
  if (BWAPI_POS_X(p) != -7 || BWAPI_POS_Y(p) != 300000) return 1;
  if (BWAPI_POS_X(BWAPI_POSITION_NONE) != BWAPI_POSITION_NONE_X) return 2;
  if (BWAPI_POS_Y(BWAPI_TILEPOSITION_UNKNOWN) != BWAPI_TILEPOSITION_UNKNOWN_Y) return 3;
  if (!BWAPI_HAS_FIELD(struct probe, a, 8)) return 4;
  if (BWAPI_HAS_FIELD(struct probe, b, 8)) return 5;
  if (BWAPI_NONE != -1) return 6;
  return 0;
}
