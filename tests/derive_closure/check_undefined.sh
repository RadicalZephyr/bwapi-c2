#!/bin/sh
# Every symbol the closure needs and does not define must be the C/C++ runtime. Anything else
# is a dependency section 10.1 says the client path does not have: Storm, Util, Boost, or a
# Win32 import beyond the seven transport names the stub provides (Appendix B).
# Usage: check_undefined.sh <object files of bwapi_c2_closure>
set -eu
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
nm -C --undefined-only "$@" | sed -n 's/^ *U //p' | sort -u > "$tmp/undef"
nm -C --defined-only   "$@" | awk '{ $1=""; $2=""; sub(/^  /, ""); print }' | sort -u > "$tmp/def"
comm -23 "$tmp/undef" "$tmp/def" > "$tmp/needed"

# The runtime: libstdc++/libc++ (std::, __cxa_*, operator new/delete, typeinfo, vtables,
# unwinding) and the handful of libc functions R6 recorded.
grep -vE '^(std::|__|operator |typeinfo|vtable |VTT |_Unwind|_GLOBAL|_ZN9__gnu_cxx|__gnu_cxx::)' "$tmp/needed" \
  | grep -vxE 'abs|mem(cmp|cpy|set|move)|str(len|ncpy|cmp)|vsnprintf|nanosleep|malloc|free|rand|sqrt|floor|ceil|pow|exp|log' \
  > "$tmp/foreign" || true

if [ -s "$tmp/foreign" ]; then
  echo "check_undefined: symbols the closure needs that are not the runtime:"
  cat "$tmp/foreign"
  exit 1
fi
echo "check_undefined: $(wc -l < "$tmp/needed") undefined symbols, all C/C++ runtime"
