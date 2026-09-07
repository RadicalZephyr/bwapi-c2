/* R12: the read-API shape cost. A stand-in for the generated ABI: a 336-byte
   record (UnitData's size) reachable either one field per FFI call, or as one
   bulk snapshot. Driven from ffi_shape.py, which supplies the host language's
   call overhead -- the thing that actually dominates a bwapi-c2 frame. */
#include <string.h>
typedef struct { int f[84]; } UnitData;        /* 336 B, sizeof(BWAPI::UnitData) */
static UnitData units[10000];                  /* GameData::units[10000] */
int get_field(int u,int f){ return units[u].f[f]; }
int snapshot(void *dst,int n){ memcpy(dst,units,(size_t)n*sizeof(UnitData)); return n; }
