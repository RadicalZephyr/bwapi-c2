#!/bin/sh
# Regenerates the R5 layout matrix. Requires clang; no linking, no Windows SDK.
# layout.cpp is a self-contained reproduction of BWAPI's shared-memory structs.
set -e
cd "$(dirname "$0")"
cat layout.cpp probe.cpp > /tmp/r5probe.cpp
for tgt in i386-pc-windows-msvc x86_64-pc-windows-msvc \
           i386-pc-windows-gnu  x86_64-pc-windows-gnu  \
           i386-unknown-linux-gnu x86_64-unknown-linux-gnu; do
  printf '%-28s ' "$tgt"
  clang++ -target "$tgt" -std=c++14 -fsyntax-only /tmp/r5probe.cpp 2>&1 \
    | grep -oE "undefined template 'SZ<[0-9]+>'" | grep -oE '[0-9]+' | paste -sd' '
done
echo
echo "order: sizeof(GameData UnitData PlayerData BulletData RegionData) offsetof(GameData.units .bullets .players .xUnitSearch BulletData.angle .isVisible)"
