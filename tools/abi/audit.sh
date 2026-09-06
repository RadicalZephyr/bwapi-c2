#!/bin/sh
# The coverage audit, off the merge path (plan section 9; implementation plan 1.6): every public
# declaration in tools/abi/audited-headers.txt is a spec entry, a rule-bearing skip, or on the
# recorded backlog in tools/abi/backlog.txt. Run by hand and at a pin bump (docs/pins.md), never
# per PR: it needs the libclang Python bindings at the same major as clang++.
#
# Usage: tools/abi/audit.sh [-v]        (pass --write-backlog after a deliberate decision)
set -eu
cd "$(dirname "$0")/../.."
exec python3 tools/abi/check_coverage.py --backlog tools/abi/backlog.txt "$@"
