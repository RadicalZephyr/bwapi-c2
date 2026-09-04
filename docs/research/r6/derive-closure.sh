#!/bin/bash
# R6: derive BWAPI's client-mode link closure by building it and asking the linker.
# Builds BWAPILIB + BWAPIClient + Shared into a static archive on Linux, then links
# upstream's own ExampleAIClient against it. Requires clang++, g++.
#
#   ./derive-closure.sh /path/to/bwapi/bwapi
set -e
B="${1:-$(cd "$(dirname "$0")/../../../bwapi" && pwd)}"
D="$(cd "$(dirname "$0")" && pwd)"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
mkdir -p "$W/obj" "$W/gen"
printf 'static const int SVN_REV = 4200;\n#include "starcraftver.h"\n' > "$W/gen/svnrev.h"

GXXINC=$(g++ -E -x c++ - -v </dev/null 2>&1 | sed -n '/#include <...>/,/End of search/p' \
         | grep '^ /' | sed 's/^ /-isystem /' | tr '\n' ' ')
# -fdelayed-template-parsing: MSVC name-lookup semantics, needed for CommandTemp.h:34
FL="-std=c++14 -O0 -c -w -fdelayed-template-parsing -nostdinc++ $GXXINC
    -I$B/include -I$B/include/BWAPI/Client -I$B/Shared -I$B/BWAPIClient/Source
    -I$W/gen -I$D/shim -I$D/patched"

# GameImpl.cpp uses `va_list&`; glibc's va_list is an array type, MSVC's is char*.
# Compile it beside a copy of Convenience.h taking va_list by value. Layout-neutral.
mkdir -p "$W/patched"
cp "$D/patched/Convenience.h" "$W/patched/"
cp "$B/BWAPIClient/Source/Command.h" "$W/patched/"
cp "$B/BWAPIClient/Source/GameImpl.cpp" "$W/patched/"

for f in "$B"/BWAPILIB/UnitCommand.cpp "$B"/BWAPILIB/Source/*.cpp \
         "$B"/Shared/*.cpp "$B"/BWAPIClient/Source/*.cpp; do
  n=$(echo "$f" | sed "s|$B/||; s|/|_|g; s|\.cpp$|.o|")
  src="$f"; [ "$(basename "$f")" = GameImpl.cpp ] && src="$W/patched/GameImpl.cpp"
  clang++ $FL "$src" -o "$W/obj/$n"
done
ar rcs "$W/libbwapi-closure.a" "$W"/obj/*.o
echo "== archive: $(ls -la "$W/libbwapi-closure.a" | awk '{print $5}') bytes, $(ls "$W"/obj | wc -l) objects"

echo "== undefined symbols outside the C++/C runtime =="
nm -uC "$W/libbwapi-closure.a" | grep -oE 'U .*' | sed 's/^U //' | sort -u \
  | grep -vE '^(std::|__|operator |typeinfo|vtable |_Unwind|_GLOBAL)' \
  | grep -vE '^(abs|mem(cmp|cpy|set|move)|str(len|ncpy)|vsnprintf|nanosleep|malloc|free)$' \
  | grep -vE '^BWAPI'
echo "== Storm / Util / Boost references =="
nm -uC "$W/libbwapi-closure.a" | grep -icE 'sfile|sbmp|smem|sdraw|storm|Util::|MemoryFrame|RemoteProcess|SharedMemory|boost' \
  | sed 's/^/  count: /'

echo "== link upstream ExampleAIClient against the closure =="
clang++ $FL "$D/win32stub.cpp" -o "$W/stub.o"
clang++ $FL "$B/ExampleAIClient/Source/ExampleAIClient.cpp" -o "$W/example.o"
g++ -o "$W/exampleaiclient" "$W/example.o" "$W/stub.o" "$W/libbwapi-closure.a"
echo "  linked OK: $(ls -la "$W/exampleaiclient" | awk '{print $5}') bytes"
timeout 5 "$W/exampleaiclient" 2>&1 | head -2 || true
