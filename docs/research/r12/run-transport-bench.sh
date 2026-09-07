#!/bin/bash
# R12: measure the transport floor for a same-host BWAPI proxy protocol -- IPC
# round-trip latency, wait-strategy jitter tails under CPU contention, the
# datagram (QUIC) packetization tax, and per-field FFI vs bulk snapshot reads.
# Linux only. Needs no BWAPI checkout and no submodules: every number here is a
# property of the OS primitives and the host language, not of BWAPI.
#   ./run-transport-bench.sh [hogs]      # hogs defaults to nproc
set -e
D="$(cd "$(dirname "$0")" && pwd)"
HOGS="${1:-$(nproc)}"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
CC="${CC:-cc}"

"$CC" -O2 -o "$W/rtt"           "$D/rtt.c"
"$CC" -O2 -o "$W/jitter"        "$D/jitter.c"
"$CC" -O2 -o "$W/packetization" "$D/packetization.c"
"$CC" -O2 -shared -fPIC -o "$W/ffi_shape.so" "$D/ffi_shape.c"

echo "### 1. Transport round trip and bulk payload"
"$W/rtt"

echo
echo "### 2. Wait-strategy jitter, idle"
"$W/jitter" 0
echo
echo "### 3. Wait-strategy jitter, $HOGS competing CPU hogs on $(nproc) cores"
"$W/jitter" "$HOGS"

echo
echo "### 4. Datagram packetization tax"
"$W/packetization"

echo
echo "### 5. Read-API shape: per-field FFI vs bulk snapshot"
python3 "$D/ffi_shape.py" "$W/ffi_shape.so"

echo
echo "### 6. AEAD throughput (is QUIC's encryption the cost? no)"
openssl speed -evp aes-128-gcm 2>/dev/null | tail -2 || echo "  openssl unavailable"
