#!/bin/bash
# R11.9: BWEM teardown under three protocols x two neutral layouts, against the PATCHED
# submodules in this repository (tools/apply-patches.sh must have run). Linux, clang++, g++.
#   docs/research/r11/run-bwem-teardown.sh
set -e
D="$(cd "$(dirname "$0")" && pwd)"; R="$D/../../.."; B="$R/third_party/bwapi/bwapi"; BW="$R/third_party/bwem/BWEM"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT; mkdir -p "$W/obj" "$W/gen"
printf 'static const int SVN_REV = 5030;\n#include "starcraftver.h"\n' > "$W/gen/svnrev.h"
GXXINC=$(g++ -E -x c++ - -v </dev/null 2>&1 | sed -n '/#include <...>/,/End of search/p' | grep '^ /' | sed 's/^ /-isystem /' | tr '\n' ' ')
FL="-std=c++14 -O1 -g -w -fdelayed-template-parsing -nostdinc++ $GXXINC
    -I$B/include -I$B/include/BWAPI/Client -I$B/Shared -I$B/BWAPIClient/Source -I$W/gen -I$D/../r6/shim -I$BW/include"
for f in "$B"/BWAPILIB/UnitCommand.cpp "$B"/BWAPILIB/Source/*.cpp "$B"/Shared/*.cpp "$B"/BWAPIClient/Source/*.cpp "$BW"/src/*.cpp; do
  clang++ $FL -c "$f" -o "$W/obj/$(echo "$f" | md5sum | cut -c1-12).o"; done
clang++ $FL -c "$D/../r6/win32stub.cpp" -o "$W/obj/stub.o"
clang++ $FL -c "$D/bwem_teardown.cpp" -o "$W/obj/fx.o"
g++ -o "$W/fx" "$W"/obj/*.o
for layout in --overlap --spaced; do for mode in --in-place --no-reset --reset; do
  set +e; "$W/fx" $layout $mode >/dev/null 2>&1; rc=$?; set -e
  printf "%-10s %-11s exit=%s\n" "$layout" "$mode" "$rc"
done; done
