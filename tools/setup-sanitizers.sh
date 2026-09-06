#!/usr/bin/env bash
#
# Provision a Linux container to build and run bwapi-c2's portable
# code under Clang's AddressSanitizer and UndefinedBehaviorSanitizer.
#
# Intended as the environment setup command for Claude Code on the web, but it
# is a plain script -- it runs the same way on any Ubuntu box.
#
# It is idempotent: re-running it on an already-provisioned container is cheap
# and harmless.
#
# Usage:
#   tools/setup-sanitizers.sh
#
set -euo pipefail

for arg in "$@"; do
  case "$arg" in
    -h|--help) awk 'NR>1 && /^#/ { sub(/^# ?/, ""); print; next } NR>1 { exit }' "$0"; exit 0 ;;
    *) echo "setup-sanitizers: unknown argument: $arg" >&2; exit 2 ;;
  esac
done

log()  { printf '\033[1m==> %s\033[0m\n' "$*"; }
warn() { printf '\033[33mwarning: %s\033[0m\n' "$*" >&2; }
die()  { printf '\033[31merror: %s\033[0m\n' "$*" >&2; exit 1; }

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
  command -v sudo >/dev/null 2>&1 || die "not root and sudo is unavailable"
  SUDO="sudo"
fi

export DEBIAN_FRONTEND=noninteractive

have_pkg() { dpkg-query -W -f='${Status}' "$1" 2>/dev/null | grep -q '^install ok installed$'; }

apt_update() {
  # Some base images carry third-party PPAs that the agent proxy blocks with a
  # 403. Those are not our packages, so their failure must not abort
  # provisioning. A package we genuinely need failing to resolve is still
  # caught -- by the install step, which does fail loudly.
  $SUDO apt-get update -qq 2>&1 | grep -viE 'ppa\.launchpad|deadsnakes|ondrej' >&2 || true
}

# --- 1. Locate the compiler, and derive its version rather than pinning one ---
# The base image's clang moves over time. Hardcoding a major version here is a
# landmine that surfaces months later as an unexplained link failure.

command -v clang++ >/dev/null 2>&1 || die "clang++ not found on PATH"
CLANG_VERSION="$(clang++ -dumpversion)"
CLANG_MAJOR="${CLANG_VERSION%%.*}"
log "clang++ ${CLANG_VERSION} (major ${CLANG_MAJOR})"

for tool in cmake ninja llvm-symbolizer; do
  command -v "$tool" >/dev/null 2>&1 \
    || warn "$tool not found on PATH; the sanitizer build will be degraded"
done

# --- 2. Install the sanitizer runtimes ---
# Ubuntu's clang ships without libclang_rt: -fsanitize=address compiles fine and
# then fails at link with "cannot find libclang_rt.asan-x86_64.a". Supplying
# that is the whole reason this script exists.
#
# Note this single package ships BOTH the x86_64 and the i386 runtime archives,
# so it covers 32-bit builds too. Do NOT be tempted to also install
# libclang-rt-N-dev:i386 to "add" 32-bit support: it is redundant, and apt
# resolves the resulting libc6-i386/libc6-amd64:i386 conflict by REMOVING the
# native package, leaving you with no x86_64 runtime at all.

RT_PKG="libclang-rt-${CLANG_MAJOR}-dev"

if have_pkg "$RT_PKG"; then
  log "$RT_PKG already installed"
else
  log "installing $RT_PKG (provides both x86_64 and i386 runtimes)"
  apt_update
  $SUDO apt-get install -y --no-install-recommends "$RT_PKG" \
    || die "could not install $RT_PKG (is clang ${CLANG_MAJOR} packaged for this release?)"
fi

# --- 3. Report what the caller now has ---

BOLD=$(printf '\033[1m'); OFF=$(printf '\033[0m')
cat <<EOF

${BOLD}Sanitizer toolchain ready.${OFF}

  clang++      $(command -v clang++)  (${CLANG_VERSION})
  runtimes     x86_64
  symbolizer   $(command -v llvm-symbolizer 2>/dev/null || echo 'MISSING - stack traces will not be symbolized')

Configure, build and run the suite the way CI's sanitized job does:

  CXX=clang++ cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \\
    -DBWAPI_C2_SANITIZERS=address,undefined
  cmake --build build-asan
  tools/test-sanitizers.sh build-asan
  export ASAN_OPTIONS=detect_leaks=1
  export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
  ctest --test-dir build-asan --output-on-failure

CMakeLists.txt is what puts -fsanitize=address,undefined -fno-omit-frame-pointer on
every target; there are no other sanitizer flags. In particular the build does not
pass -fno-sanitize-recover, so a UBSan diagnostic is recoverable and the process
carries on and exits 0 without the UBSAN_OPTIONS=halt_on_error=1 above. Set it, or
the suite goes green over reported undefined behaviour.
EOF
