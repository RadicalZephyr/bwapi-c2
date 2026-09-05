// The one number plan section 10.2 rests on, asserted against the real pinned headers at
// compile time, so a pin bump that changes GameData cannot build. dump_layout.py checks the
// same on every target and field by field; this is the belt to its braces.
#include <BWAPI/Client/GameData.h>

static_assert(sizeof(BWAPI::GameData) == 33017048, "GameData layout changed: revisit plan section 10.2");
static_assert(offsetof(BWAPI::GameData, client_version) == 0, "client_version must stay first");
static_assert(offsetof(BWAPI::GameData, revision) == 4, "revision must stay second");
