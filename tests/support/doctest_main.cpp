// The one translation unit that defines doctest's runtime and main(). Every test executable
// links this object plus its own test sources. doctest (tests/support/doctest.h) is vendored:
// single header, MIT, reports the failing assertion, and unlike googletest is not something
// BWEM's submodules already carry a copy of that we refuse to fetch (judgment call 3).
//
// main() also installs the library's own after-the-pump step on the Fixture, once, so every
// fixture-driven suite pumps the way bwapi_client_update() does without replaying it by hand.
// Every bwapi_c2_add_test() binary links the ABI's objects beside this one, which is what
// makes the internal reachable here.
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include "abi_internal.h"
#include "fixture.h"

int main(int argc, char** argv) {
  bwapi_c2::test::Fixture::set_after_pump(bwapi_c2::after_pump);
  return doctest::Context(argc, argv).run();
}
