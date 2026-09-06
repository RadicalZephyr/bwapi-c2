#!/bin/bash
# R12: the section-4 struct-array convention against a reused buffer. Reproduces
# abi_internal.h's write_rows()/write_row()/write_struct() verbatim and drives them as a
# consumer reusing one buffer across frames, for three consumer/library size pairings.
# Linux, clang++. No submodules, no BWAPI.
#   docs/research/r12/run-stride-reuse.sh
set -e
D="$(cd "$(dirname "$0")" && pwd)"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
clang++ -std=c++17 -O1 -g -Wall -Wextra -o "$W/stride" "$D/stride_reuse.cpp"
"$W/stride"
