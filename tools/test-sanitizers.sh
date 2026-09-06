#!/usr/bin/env bash
#
# --- Prove the sanitizers actually fire ---
#
# Confirming a package installed is not the same as confirming that
# -fsanitize=address links and traps. Verify the real behaviour, so a broken
# container fails here, loudly, rather than halfway through a debugging session.
set -euo pipefail

log()  { printf '\033[1m==> %s\033[0m\n' "$*"; }
warn() { printf '\033[33mwarning: %s\033[0m\n' "$*" >&2; }
die()  { printf '\033[31merror: %s\033[0m\n' "$*" >&2; exit 1; }

SMOKE_DIR="$(mktemp -d)"
trap 'rm -rf "$SMOKE_DIR"' EXIT

cat > "$SMOKE_DIR/asan.cpp" <<'EOF'
#include <cstdio>
int main() {
  int *p = new int[4];
  std::printf("%d\n", p[5]);          // heap-buffer-overflow
  delete[] p;
}
EOF

# Kept runtime-dependent on argc: a constant-folded overflow is diagnosed at
# compile time and would never exercise the runtime library we are testing.
cat > "$SMOKE_DIR/ubsan.cpp" <<'EOF'
int mul(int a, int b) { return a * b; }
int main(int argc, char **) {
  return mul(2000000000, 1 + argc) & 1;   // signed integer overflow
}
EOF


smoke() {
  local arch_flag="$1" label="$2" out rc

  out="$SMOKE_DIR/asan_$label"
  # shellcheck disable=SC2086  # arch_flag is intentionally word-split (empty or -m32)
  clang++ $arch_flag -std=c++17 -g -fsanitize=address -fno-omit-frame-pointer \
      "$SMOKE_DIR/asan.cpp" -o "$out" \
    || die "[$label] ASan failed to build or link"
  # Redirect to a file rather than piping into grep: the program is *meant* to
  # abort, and under `set -o pipefail` that nonzero status would fail the
  # pipeline even when grep matched.
  set +e
  ASAN_OPTIONS=detect_leaks=0 "$out" >"$SMOKE_DIR/as.log" 2>&1
  rc=$?
  set -e
  grep -q 'heap-buffer-overflow' "$SMOKE_DIR/as.log" \
    || die "[$label] ASan linked but did not report a known heap-buffer-overflow"
  [ "$rc" -ne 0 ] \
    || die "[$label] ASan reported but did not abort"
  log "[$label] ASan reports heap-buffer-overflow and aborts"

  out="$SMOKE_DIR/ubsan_$label"
  # shellcheck disable=SC2086
  clang++ $arch_flag -std=c++17 -g -fsanitize=undefined -fno-sanitize-recover=all \
      -fno-omit-frame-pointer "$SMOKE_DIR/ubsan.cpp" -o "$out" \
    || die "[$label] UBSan failed to build or link"
  set +e
  UBSAN_OPTIONS=print_stacktrace=1 "$out" >"$SMOKE_DIR/ub.log" 2>&1
  rc=$?
  set -e
  grep -q 'signed integer overflow' "$SMOKE_DIR/ub.log" \
    || die "[$label] UBSan linked but did not report a known signed overflow"
  [ "$rc" -ne 0 ] \
    || die "[$label] UBSan reported but did not abort; -fno-sanitize-recover is not in effect"
  log "[$label] UBSan reports signed integer overflow and aborts"
}

log "verifying sanitizers against known faults"
smoke "" "x86_64"
