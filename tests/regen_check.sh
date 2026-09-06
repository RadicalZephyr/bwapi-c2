#!/bin/sh
# The regenerate-and-diff check (implementation plan rule 2 and 1.4): run every emitter and fail
# if any checked-in generated file changes. A CI step, not a CTest test: it needs the spec's
# Python and a clean git tree, not a built library.
#
# Usage: tests/regen_check.sh        (from anywhere; needs python3 with PyYAML)
set -eu
cd "$(dirname "$0")/.."

python3 tools/abi/regen.py

# Everything the emitters write that is checked in. bwapi_c2_bwem.h joins in phase 3.
generated="include/bwapi_c2.h include/bwapi_c2_types.h src/*.gen.cpp bwapi_c2.def api.json"

# shellcheck disable=SC2086
if ! git diff --exit-code -- $generated; then
  echo "regen_check: the files above are out of date; run tools/abi/regen.py and commit the result" >&2
  exit 1
fi
# A generated file that is not tracked at all is the same mistake.
# shellcheck disable=SC2086
untracked=$(git ls-files --others --exclude-standard -- $generated)
if [ -n "$untracked" ]; then
  echo "regen_check: generated but not committed:" >&2
  echo "$untracked" >&2
  exit 1
fi
echo "regen_check: every generated file is current"
