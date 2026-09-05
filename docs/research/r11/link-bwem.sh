#!/bin/bash
# R11.4: does BWEM link against the client-only BWAPI closure derived in R6?
# Compiles BWEM's 14 TUs against the closure's include set, archives both, and
# links a program that touches the BWEM singleton.
#   ./link-bwem.sh /path/to/bwapi/bwapi /path/to/BWEM-community/BWEM
set -e
B="${1:-$(cd "$(dirname "$0")/../../../bwapi" && pwd)}"
BW="${2:?usage: link-bwem.sh <bwapi/bwapi> <BWEM-community/BWEM>}"
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

echo "== compiling BWEM (14 TUs) against the closure include set"
for f in "$BW"/src/*.cpp; do clang++ $FL -c "$f" -o "$W/objbwem/$(basename "$f" .cpp).o"; done
ar rcs "$W/libbwem.a" "$W"/objbwem/*.o
echo "   libbwem.a $(stat -c%s "$W/libbwem.a") bytes"

echo "== BWAPI symbols BWEM requires"
nm -uC "$W/libbwem.a" | grep -oE 'U .*' | sed 's/^U //' | sort -u | grep '^BWAPI::' | sed 's/^/   /'
echo "== anything outside BWAPI:: and BWEM:: (excluding the C/C++ runtime)"
nm -uC "$W/libbwem.a" | grep -oE 'U .*' | sed 's/^U //' | sort -u \
  | grep -vE '^(std::|__|operator |typeinfo|vtable |_Unwind|_GLOBAL|BWAPI::|BWEM::)' \
  | grep -vE '^(abs|mem(cmp|cpy|set|move)|str(len|ncpy|cmp)|v?snprintf|malloc|free|floor|ceil|sqrt|pow|atan2)$' \
  | sed 's/^/   /'
echo "== Storm / Util / Boost references"
echo -n "   count: "; nm -uC "$W/libbwem.a" | grep -icE 'sfile|sbmp|smem|sdraw|storm|Util::|MemoryFrame|RemoteProcess|SharedMemory|boost' || true

echo "== building the BWAPI closure and linking"
for f in "$B"/BWAPILIB/UnitCommand.cpp "$B"/BWAPILIB/Source/*.cpp "$B"/Shared/*.cpp "$B"/BWAPIClient/Source/*.cpp; do
  n=$(echo "$f" | sed "s|$B/||; s|/|_|g; s|\.cpp$|.o|")
  src="$f"; [ "$(basename "$f")" = GameImpl.cpp ] && src="$W/patched/GameImpl.cpp"
  clang++ $FL -c "$src" -o "$W/obj/$n"
done
ar rcs "$W/libclosure.a" "$W"/obj/*.o
clang++ $FL -c "$R6/win32stub.cpp" -o "$W/stub.o"
clang++ $FL -c "$D/bwemlink.cpp"   -o "$W/bwemlink.o"
g++ -o "$W/bwemlink" "$W/bwemlink.o" "$W/stub.o" "$W/libbwem.a" "$W/libclosure.a"
echo "   linked $(stat -c%s "$W/bwemlink") bytes"
"$W/bwemlink"
