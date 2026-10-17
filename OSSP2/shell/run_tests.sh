#!/bin/bash
# Run the ForgeShell test suite N times (default 3), report pass/fail per run.
cd "$(dirname "$0")" || exit 1
N=${1:-3}
fails=0
for i in $(seq 1 "$N"); do
  python3 test_shell.py > "/tmp/fs_test_$i.log" 2>&1
  rc=$?
  p=$(grep -c '  PASS' "/tmp/fs_test_$i.log")
  f=$(grep -c '  FAIL' "/tmp/fs_test_$i.log")
  echo "run $i: exit=$rc passed=$p failed=$f"
  if [ "$rc" -ne 0 ]; then
    fails=1
    tail -5 "/tmp/fs_test_$i.log"
  fi
done
exit $fails
