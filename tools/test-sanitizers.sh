#!/bin/bash
# Verify that a sanitized bwapi-c2 build would actually report an error, rather than merely
# having asked for one. A green `ctest` under -fsanitize=address,undefined means nothing unless
# the instrumentation is in the objects and the runtimes are on the link line, so this checks
# both halves: every artifact in the build is instrumented, and a deliberate fault compiled the
# way this build compiles code is diagnosed and exits non-zero.
#   tools/test-sanitizers.sh [build-dir]        # default build-dir: build
# The build directory must be configured with -DBWAPI_C2_SANITIZERS=... and already built.
set -u
R="$(cd "$(dirname "$0")/.." && pwd)"
if [ $# -ge 1 ]; then case "$1" in /*) B="$1";; *) B="$1"; [ -d "$B" ] || B="$R/$1";; esac; else B="$R/build"; fi
[ -d "$B" ] && B="$(cd "$B" && pwd)"
CACHE="$B/CMakeCache.txt"

if [ ! -f "$CACHE" ]; then
  echo "no CMakeCache.txt in $B; configure and build first:" >&2
  echo "  CXX=clang++ cmake -B build -G Ninja -DBWAPI_C2_SANITIZERS=address,undefined && cmake --build build" >&2
  exit 2
fi
CXX=$(sed -n 's/^CMAKE_CXX_COMPILER:[A-Z]*=//p' "$CACHE")
SAN=$(sed -n 's/^BWAPI_C2_SANITIZERS:[A-Z]*=//p' "$CACHE")
if [ -z "$SAN" ]; then
  echo "$B was configured with BWAPI_C2_SANITIZERS empty; nothing to verify." >&2
  echo "  CXX=clang++ cmake -B $B -G Ninja -DBWAPI_C2_SANITIZERS=address,undefined" >&2
  exit 2
fi
case ",$SAN," in *,address,*) HAVE_ASAN=1;; *) HAVE_ASAN=0;; esac
case ",$SAN," in *,undefined,*) HAVE_UBSAN=1;; *) HAVE_UBSAN=0;; esac
if [ "$HAVE_ASAN$HAVE_UBSAN" = 00 ]; then
  echo "$B was configured with BWAPI_C2_SANITIZERS=$SAN, which has neither address nor" >&2
  echo "undefined in it; this script has no probe for it and will not claim one passed." >&2
  exit 2
fi
echo "build   $B"
echo "cxx     $CXX"
echo "sanitizers  $SAN"
echo

fails=0
report() { # name pass-or-fail detail
  if [ "$2" = pass ]; then printf '  %-46s pass  %s\n' "$1" "${3:-}"
  else printf '  %-46s FAIL  %s\n' "$1" "${3:-}"; fails=$((fails + 1)); fi
}

# --- 1. the build's own artifacts carry the instrumentation ------------------------------------
# Every translation unit compiled into this build calls into the ASan runtime: -fsanitize=address
# instruments every load and store, so an object with no __asan_ reference was compiled without
# it. UBSan is not checked per-object -- a trivial TU (the header-hygiene stubs, the layout
# static_asserts) has nothing for it to check -- so it is checked per linked image instead, where
# -fsanitize=undefined pulls the runtime in unconditionally. CMake's own compiler-probe binaries
# under CMakeFiles/<version>/ predate the cache variable and are not part of the build.
ours() { case "${1#$B/}" in CMakeFiles/[0-9]*) return 1;; *) return 0;; esac; }
echo "instrumentation in $B"
if [ "$HAVE_ASAN" = 1 ]; then
  objs=0; bare=0
  while IFS= read -r o; do
    ours "$o" || continue
    objs=$((objs + 1))
    nm -u "$o" 2>/dev/null | grep -q '__asan' || { bare=$((bare + 1)); echo "      no ASan instrumentation: ${o#$B/}"; }
  done < <(find "$B" -name '*.o' -type f)
  if [ "$objs" -eq 0 ]; then report "objects instrumented" fail "no object files; build first"
  elif [ "$bare" -eq 0 ]; then report "objects instrumented" pass "$objs/$objs"
  else report "objects instrumented" fail "$((objs - bare))/$objs"; fi
fi

imgs=0; bare_a=0; bare_u=0
while IFS= read -r f; do
  ours "$f" || continue
  case "$(file -b "$f")" in *ELF*executable*|*ELF*shared\ object*) ;; *) continue;; esac
  imgs=$((imgs + 1)); syms=$(nm "$f" 2>/dev/null)
  if [ "$HAVE_ASAN" = 1 ]; then grep -q '__asan_init' <<<"$syms" || { bare_a=$((bare_a + 1)); echo "      no ASan runtime: ${f#$B/}"; }; fi
  if [ "$HAVE_UBSAN" = 1 ]; then grep -q '__ubsan_handle' <<<"$syms" || { bare_u=$((bare_u + 1)); echo "      no UBSan runtime: ${f#$B/}"; }; fi
done < <(find "$B" -type f -perm -u+x -o -type f -name '*.so')
if [ "$imgs" -eq 0 ]; then report "runtimes linked into images" fail "no linked images; build first"
else
  [ "$HAVE_ASAN" = 1 ] && { [ "$bare_a" -eq 0 ] && report "ASan runtime linked" pass "$imgs/$imgs images" || report "ASan runtime linked" fail "$((imgs - bare_a))/$imgs images"; }
  [ "$HAVE_UBSAN" = 1 ] && { [ "$bare_u" -eq 0 ] && report "UBSan runtime linked" pass "$imgs/$imgs images" || report "UBSan runtime linked" fail "$((imgs - bare_u))/$imgs images"; }
fi
echo

# --- 2. a deliberate fault is reported ---------------------------------------------------------
# Compiled with what CMakeLists.txt applies to every target (add_compile_options /
# add_link_options), and run under the ASAN_OPTIONS/UBSAN_OPTIONS the CI job sets, so a pass here
# is a statement about the configuration the suite runs under and not about clang in general.
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
FL="-std=c++17 -g -O1 -w -fsanitize=$SAN -fno-omit-frame-pointer"
export ASAN_OPTIONS=detect_leaks=1
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1

cat > "$W/use_after_free.cpp" <<'EOF'
// ASan: read a heap block after it is freed.
#include <cstdio>
#include <cstdlib>
int main(int argc, char **) {
  int *p = static_cast<int *>(std::malloc(8 * sizeof(int)));
  p[argc] = 42;
  std::free(p);
  std::printf("%d\n", p[argc]);
  return 0;
}
EOF
cat > "$W/signed_overflow.cpp" <<'EOF'
// UBSan: overflow a signed int, which is undefined and not a trap on x86-64.
#include <climits>
#include <cstdio>
int main(int argc, char **) {
  volatile int big = INT_MAX;
  int sum = big + argc;
  std::printf("%d\n", sum);
  return 0;
}
EOF
cat > "$W/leak.cpp" <<'EOF'
// LeakSanitizer (ASan's detect_leaks=1): drop the only reference to a heap block.
#include <cstdlib>
#include <cstring>
static __attribute__((noinline)) void lose(unsigned n) {
  void *p = std::malloc(n);
  std::memset(p, 0x5a, n);
  *reinterpret_cast<void *volatile *>(&p) = nullptr;  // the compiler-rt idiom: hide the pointer
}
int main(int argc, char **) {
  lose(1337u * static_cast<unsigned>(argc));
  return 0;
}
EOF

# probe-name  needed-sanitizer  expected diagnostic
probe() {
  local name="$1" need="$2" want="$3"
  [ "$need" = 1 ] || return 0
  if ! "$CXX" $FL "$W/$name.cpp" -o "$W/$name" 2>"$W/$name.build"; then
    report "$name" fail "did not compile; see below"; sed 's/^/      /' "$W/$name.build"; return
  fi
  local out rc
  out=$("$W/$name" 2>&1); rc=$?
  if [ "$rc" -eq 0 ]; then report "$name" fail "exited 0; no diagnostic"
  elif grep -qF "$want" <<<"$out"; then report "$name" pass "exit=$rc  \"$want\""
  else report "$name" fail "exit=$rc but no \"$want\" in the output"; sed 's/^/      /' <<<"$out" | head -5; fi
}

echo "deliberate faults"
probe use_after_free  "$HAVE_ASAN"  "heap-use-after-free"
probe signed_overflow "$HAVE_UBSAN" "signed integer overflow"
probe leak            "$HAVE_ASAN"  "detected memory leaks"
echo

if [ "$fails" -eq 0 ]; then echo "sanitizers are live in $B"; exit 0; fi
echo "$fails check(s) failed: a green suite in $B does not mean what it looks like" >&2
exit 1
