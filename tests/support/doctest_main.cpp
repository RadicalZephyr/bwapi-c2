// The one translation unit that defines doctest's runtime and main(). Every test executable
// links this object plus its own test sources. doctest (tests/support/doctest.h) is vendored:
// single header, MIT, reports the failing assertion, and unlike googletest is not something
// BWEM's submodules already carry a copy of that we refuse to fetch (judgment call 3).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
