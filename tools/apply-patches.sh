#!/bin/sh
# Apply the carried patches under patches/ to the pinned submodules (plan section 15.2,
# docs/pins.md). Idempotent: a patch already applied is skipped; one that applies is applied;
# one that does neither fails loudly, which at a pin bump is the bump's first finding.
#
#   tools/apply-patches.sh            apply all
#   tools/apply-patches.sh --reverse  remove all (before moving a pin)
#
# A patch file is named <dep>-<what>.patch and targets third_party/<dep>. Prose before the
# first `diff --git` line is ignored by git apply and is where the patch explains itself.
# CMake configure runs this script (implementation plan 0.5); it can also be run by hand.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
mode=${1:-apply}
case "$mode" in
  apply|--reverse) ;;
  *) echo "usage: $0 [--reverse]" >&2; exit 2 ;;
esac

status=0
for patch in "$root"/patches/*.patch; do
  name=$(basename "$patch")
  dep=${name%%-*}
  tree="$root/third_party/$dep"
  if [ ! -f "$tree/.git" ] && [ ! -d "$tree/.git" ]; then
    echo "apply-patches: $name: submodule third_party/$dep is not checked out" >&2
    status=1; continue
  fi
  if [ "$mode" = "--reverse" ]; then
    if git -C "$tree" apply --check --reverse "$patch" 2>/dev/null; then
      git -C "$tree" apply --reverse "$patch"
      echo "apply-patches: $name: removed"
    elif git -C "$tree" apply --check "$patch" 2>/dev/null; then
      echo "apply-patches: $name: not applied, nothing to remove"
    else
      echo "apply-patches: $name: neither applied nor removable against third_party/$dep" >&2
      status=1
    fi
    continue
  fi
  if git -C "$tree" apply --check --reverse "$patch" 2>/dev/null; then
    echo "apply-patches: $name: already applied"
  elif git -C "$tree" apply --check "$patch" 2>/dev/null; then
    git -C "$tree" apply "$patch"
    echo "apply-patches: $name: applied"
  else
    echo "apply-patches: $name: does not apply to third_party/$dep at $(git -C "$tree" rev-parse --short HEAD)" >&2
    echo "  Move the pin? Re-derive the patch (docs/pins.md, 'Moving a pin'). Detail:" >&2
    git -C "$tree" apply --check "$patch" 2>&1 | sed 's/^/  /' >&2 || true
    status=1
  fi
done
exit $status
