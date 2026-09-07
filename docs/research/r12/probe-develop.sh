#!/bin/bash
# R12: does BWAPI 5 (upstream bwapi/bwapi@develop) still compile the pinned BWEM, and does its
# client library compile on Linux without the two MSVC-isms section 10.1 works around? Syntax
# checks only: the network layer needs protobuf fetched at configure time and is not probed.
#   docs/research/r12/probe-develop.sh <checkout of bwapi/bwapi at develop>
set -e
D="$(cd "$(dirname "$0")" && pwd)"; R="$D/../../.."; BW="$R/third_party/bwem/BWEM"
DEV="${1:?usage: probe-develop.sh <develop checkout>}"
[ -f "$DEV/include/BWAPI.h" ] || { echo "no include/BWAPI.h under $DEV"; exit 1; }
[ -f "$BW/include/bwem.h" ] || { echo "BWEM submodule missing; git submodule update --init --depth 1"; exit 1; }

echo "BWEM's 14 TUs against develop's headers (clang++ -std=c++17, no -fdelayed-template-parsing):"
ok=0; fail=0
for f in "$BW"/src/*.cpp; do
  if clang++ -std=c++17 -fsyntax-only -w -DNOMINMAX=1 -I"$DEV/include" -I"$BW/include" "$f" 2>/dev/null; then ok=$((ok+1)); else fail=$((fail+1)); echo "  FAIL $(basename "$f")"; fi
done
echo "  OK=$ok FAIL=$fail"

echo "develop's Library/BWAPILIB (every TU) on Linux:"
for cxx in clang++ g++; do
  ok=0; fail=0
  for f in "$DEV"/Library/BWAPILIB/Source/*/*.cpp; do
    if $cxx -std=c++17 -fsyntax-only -w -I"$DEV/include" "$f" 2>/dev/null; then ok=$((ok+1)); else fail=$((fail+1)); echo "  FAIL $cxx $(basename "$f")"; fi
  done
  echo "  $cxx: OK=$ok FAIL=$fail"
done

echo "the two section-10.1 MSVC-isms on develop:"
echo "  CommandTemp.h present: $([ -f "$DEV/include/BWAPI/Client/CommandTemp.h" ] && echo yes || echo no)"
echo "  va_list by reference in Convenience.h: $(grep -c 'va_list *&' "$DEV/Library/BWAPILIB/Source/Convenience.h" 2>/dev/null || true)"
echo "  Win32 imports outside the 1.16.1 backend: $(grep -rlE 'CreateFileMapping|CreateNamedPipe|MapViewOfFile' "$DEV/include" "$DEV/Library" "$DEV/Network/BWAPIFrontendClient" "$DEV/Network/BWAPINetworkCore" 2>/dev/null | wc -l) files"
