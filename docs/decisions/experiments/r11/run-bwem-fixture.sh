#!/bin/bash
# R11.6: drive BWEM's full map analysis from a SYNTHETIC GameData.
# No StarCraft, no MPQs, no map file, no server, no Blizzard data of any kind.
#   ./run-bwem-fixture.sh <bwapi/bwapi> <BWEM-community/BWEM> [--reinit|--exit-clean]
set -e
B="${1:?usage: run-bwem-fixture.sh <bwapi/bwapi> <BWEM-community/BWEM> [flag]}"
BW="${2:?}"; FLAG="${3:---exit-clean}"
D="$(cd "$(dirname "$0")" && pwd)"; R6="$D/../r6"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
mkdir -p "$W/obj" "$W/objbwem" "$W/gen" "$W/patched"
printf 'static const int SVN_REV = 4200;\n#include "starcraftver.h"\n' > "$W/gen/svnrev.h"
cp "$R6/patched/Convenience.h" "$W/patched/"
cp "$B/BWAPIClient/Source/Command.h" "$B/BWAPIClient/Source/GameImpl.cpp" "$W/patched/"
GXXINC=$(g++ -E -x c++ - -v </dev/null 2>&1 | sed -n '/#include <...>/,/End of search/p' \
         | grep '^ /' | sed 's/^ /-isystem /' | tr '\n' ' ')
FL="-std=c++14 -O2 -w -fdelayed-template-parsing -nostdinc++ $GXXINC
    -I$B/include -I$B/include/BWAPI/Client -I$B/Shared -I$B/BWAPIClient/Source
    -I$W/gen -I$R6/shim -I$R6/patched -I$BW/include"
for f in "$BW"/src/*.cpp; do clang++ $FL -c "$f" -o "$W/objbwem/$(basename "$f" .cpp).o"; done
ar rcs "$W/libbwem.a" "$W"/objbwem/*.o
for f in "$B"/BWAPILIB/UnitCommand.cpp "$B"/BWAPILIB/Source/*.cpp "$B"/Shared/*.cpp "$B"/BWAPIClient/Source/*.cpp; do
  n=$(echo "$f" | sed "s|$B/||; s|/|_|g; s|\.cpp$|.o|")
  src="$f"; [ "$(basename "$f")" = GameImpl.cpp ] && src="$W/patched/GameImpl.cpp"
  clang++ $FL -c "$src" -o "$W/obj/$n"
done
ar rcs "$W/libclosure.a" "$W"/obj/*.o
clang++ $FL -c "$R6/win32stub.cpp"    -o "$W/stub.o"
clang++ $FL -c "$D/bwem_fixture.cpp"  -o "$W/fx.o"
g++ -o "$W/fx" "$W/fx.o" "$W/stub.o" "$W/libbwem.a" "$W/libclosure.a"
"$W/fx" "$FLAG"
