#!/usr/bin/env bash
# Runs the E2E test under gdb until it crashes (or N runs pass) and saves the
# backtrace of the first crash. Used to hunt the intermittent exit-time crash.
#   bash tools/e2e_gdb_hunt.sh 30
cd "$(dirname "$0")/.." || exit 1
runs=${1:-30}
root=$(pwd -W)
for i in $(seq 1 "$runs"); do
  log="build/gdb_run.log"
  gdb -batch -ex "set pagination off" -ex run -ex "bt 40" -ex "info threads" -ex "thread apply all bt 15" \
      --args build/e2e_section12.exe "$root/build/cafeteria_server.exe" "$root" > "$log" 2>&1
  taskkill //F //IM cafeteria_server.exe > /dev/null 2>&1
  if grep -qE "received signal|exception|Unknown signal|SIGSEGV|SIGABRT|0xc0000374" "$log"; then
    cp "$log" build/gdb_crash.log
    echo "CRASH on run $i: build/gdb_crash.log"
    exit 1
  fi
  echo "run $i: $(grep -E 'checks,' "$log" | tail -1) $(grep -oE 'exited (normally|with code [0-9]+)' "$log" | tail -1)"
done
echo "no crash in $runs runs"
