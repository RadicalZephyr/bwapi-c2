#!/bin/bash
# R7: drive the real BWAPI client (R6's 44-TU closure) against a synthetic
# GameData -- no server, no shared memory, no pipe, no MPQs, no StarCraft.
#   ./run-fixture-harness.sh /path/to/bwapi/bwapi
set -e
B="${1:-$(cd "$(dirname "$0")/../../../bwapi" && pwd)}"
D="$(cd "$(dirname "$0")" && pwd)"; R6="$D/../r6"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
mkdir -p "$W/obj" "$W/gen" "$W/patched"
printf 'static const int SVN_REV = 4200;\n#include "starcraftver.h"\n' > "$W/gen/svnrev.h"
cp "$R6/patched/Convenience.h" "$W/patched/"
cp "$B/BWAPIClient/Source/Command.h" "$B/BWAPIClient/Source/GameImpl.cpp" "$W/patched/"

GXXINC=$(g++ -E -x c++ - -v </dev/null 2>&1 | sed -n '/#include <...>/,/End of search/p' \
         | grep '^ /' | sed 's/^ /-isystem /' | tr '\n' ' ')
FL="-std=c++14 -O0 -w -fdelayed-template-parsing -nostdinc++ $GXXINC
    -I$B/include -I$B/include/BWAPI/Client -I$B/Shared -I$B/BWAPIClient/Source
    -I$W/gen -I$R6/shim -I$R6/patched"

for f in "$B"/BWAPILIB/UnitCommand.cpp "$B"/BWAPILIB/Source/*.cpp \
         "$B"/Shared/*.cpp "$B"/BWAPIClient/Source/*.cpp; do
  n=$(echo "$f" | sed "s|$B/||; s|/|_|g; s|\.cpp$|.o|")
  src="$f"; [ "$(basename "$f")" = GameImpl.cpp ] && src="$W/patched/GameImpl.cpp"
  clang++ $FL -c "$src" -o "$W/obj/$n"
done
ar rcs "$W/libclosure.a" "$W"/obj/*.o
clang++ $FL -c "$R6/win32stub.cpp"   -o "$W/stub.o"
clang++ $FL -c "$D/fixture_harness.cpp" -o "$W/harness.o"
g++ -o "$W/harness" "$W/harness.o" "$W/stub.o" "$W/libclosure.a"
"$W/harness"
