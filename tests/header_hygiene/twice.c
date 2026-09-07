/* All three public headers, each twice, as C99. Exercises the include guards and the macros
 * with the C preprocessor and type system. */
#include "bwapi_c2_types.h"
#include "bwapi_c2.h"
#include "bwapi_c2_bwem.h"
#include "bwapi_c2_types.h"
#include "bwapi_c2.h"
#include "bwapi_c2_bwem.h"

#include <stddef.h>

/* Every operand is a compile-time constant; the volatile copies keep these runtime checks
 * rather than constant conditions MSVC's /W4 rejects (C4127). */
int main(void) {
  volatile bwapi_position p = BWAPI_POS_MAKE(-7, 300000);
  volatile bwapi_position none = BWAPI_POSITION_NONE;
  volatile bwapi_position unknown_tile = BWAPI_TILEPOSITION_UNKNOWN;
  volatile int32_t none_id = BWAPI_NONE;
  /* A struct is exactly its fields (plan section 4): the first table row starts with its id. */
  volatile size_t id_offset = offsetof(bwapi_race_row, id);
  if (BWAPI_POS_X(p) != -7 || BWAPI_POS_Y(p) != 300000) return 1;
  if (BWAPI_POS_X(none) != BWAPI_POSITION_NONE_X) return 2;
  if (BWAPI_POS_Y(unknown_tile) != BWAPI_TILEPOSITION_UNKNOWN_Y) return 3;
  if (id_offset != 0) return 4;
  if (none_id != -1) return 6;
  return 0;
}
