#!/bin/sh
# Plan section 1.4 and 10.2: nothing in the closure may change struct packing. Twenty
# `#pragma pack` directives exist in the BWAPI tree, all in the injected DLL and Storm, which
# the closure excludes; the closure's own directories must have none, and no project file of
# theirs may set /Zp. Usage: check_no_pack.sh <bwapi/bwapi> <BWEM>
set -eu
B="$1"; BW="$2"
hits=$(grep -rn -e '#pragma pack' -e '/Zp' -e '<StructMemberAlignment>' \
  "$B/include" "$B/BWAPILIB" "$B/Shared" "$B/BWAPIClient" "$BW/include" "$BW/src" || true)
if [ -n "$hits" ]; then echo "check_no_pack: packing directives in the closure:"; echo "$hits"; exit 1; fi
echo "check_no_pack: no packing directive in the closure"
