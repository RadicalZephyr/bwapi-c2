#!/bin/bash
# R13: the cost of the per-frame event snapshot, against the list walk it must do anyway and
# the drain it enables. Builds this repository Release into a scratch directory with the
# benchmark target enabled, runs it, and cleans up. Linux, clang++. Needs the submodules.
#   docs/research/r13/run-event-snapshot-bench.sh
set -e
R="$(cd "$(dirname "$0")/../../.." && pwd)"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
CXX=clang++ cmake -S "$R" -B "$W" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBWAPI_C2_BENCH=ON >/dev/null
cmake --build "$W" --target r13_event_snapshot_bench >/dev/null
"$W/tests/r13_event_snapshot_bench"
