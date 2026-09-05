#!/bin/bash
# R8: run SWIG's experimental C target against BWAPI's headers.
# Downloads and builds SWIG 4.5.1 (no bison needed -- the release tarball
# ships a pre-generated parser), then runs every .i file in this directory.
#   ./run-swig-experiment.sh /path/to/bwapi/bwapi
set -e
B="${1:-$(cd "$(dirname "$0")/../../../bwapi" && pwd)}"
D="$(cd "$(dirname "$0")" && pwd)"
W="${SWIG_WORK:-$(mktemp -d)}"
if [ ! -x "$W/swig-4.5.1/swig" ]; then
  echo "== fetching and building SWIG 4.5.1 into $W"
  curl -sL -o "$W/swig.tar.gz" \
    "https://downloads.sourceforge.net/project/swig/swig/swig-4.5.1/swig-4.5.1.tar.gz"
  tar xzf "$W/swig.tar.gz" -C "$W"
  ( cd "$W/swig-4.5.1" && ./configure --without-pcre >/dev/null && make -j"$(nproc)" >/dev/null )
fi
export SWIG_LIB="$W/swig-4.5.1/Lib"; SW="$W/swig-4.5.1/swig"
"$SW" -version | head -2
out="$W/out"; mkdir -p "$out"; cd "$out"
for i in "$D"/*.i; do
  n=$(basename "$i" .i)
  printf '\n===== %s\n' "$n"
  if "$SW" -c++ -c -I"$B/include" -outdir . -o "$out/${n}_wrap.cxx" "$i" > "$n.log" 2>&1; then
    printf '  swig: OK   exports=%s\n' "$(grep -cE '^SWIGIMPORT' ${n}_wrap.h 2>/dev/null || echo 0)"
  else
    printf '  swig: FAILED (exit %s)\n' "$?"
  fi
  grep -oE 'Warning [0-9]+|Error:' "$n.log" | sort | uniq -c | sed 's/^/    /'
  grep -E '^.*Error:' "$n.log" | head -2 | sed 's/^/    /'
done
printf '\n===== namespaced-enum collision test\n'
printf '#include "unittype_wrap.h"\n#include "weapontype_wrap.h"\nint main(void){return 0;}\n' > both.c
gcc -std=c99 -fsyntax-only both.c 2>&1 | head -6 | sed 's/^/    /' || true
