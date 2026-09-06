#!/bin/sh
# Run upstream's ExampleAIClient, linked against our closure, for two seconds. With the Linux
# stub every transport call fails, so it prints "Connecting...", then Client::connect() reports
# the missing game table once a second, forever. Seeing both lines proves the closure links
# and the client's entry path executes; the timeout is the expected exit.
set -u
exe="$1"
out=$(timeout 2 "$exe" 2>&1)
rc=$?
if [ "$rc" -ne 124 ] && [ "$rc" -ne 0 ]; then
  echo "run_example: exited with $rc before the timeout:"; echo "$out"; exit 1
fi
case "$out" in
  *"Connecting..."*"Game table mapping not found."*) echo "run_example: reached connect() and the transport"; exit 0 ;;
  *) echo "run_example: expected the Connecting... and game-table lines, got:"; echo "$out"; exit 1 ;;
esac
