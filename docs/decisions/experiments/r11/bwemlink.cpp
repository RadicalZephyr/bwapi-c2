// R11.4: does BWEM link against the client-only BWAPI closure?
#include <BWAPI.h>
#include <BWAPI/Client/GameImpl.h>
#include <bwem.h>
#include <cstdio>
int main() {
  printf("BWEM linked. Map singleton at %p\n", (void*)&BWEM::Map::Instance());
  printf("  Initialized() = %d\n", (int)BWEM::Map::Instance().Initialized());
  return 0;
}
