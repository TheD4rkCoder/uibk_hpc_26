#!/usr/bin/env bash
# Reproduce both original hangs and verify the fixed versions.
# Requires an MPI toolchain; the workspace-local one is picked up if present.
set -u
cd "$(dirname "$0")"
[ -f .mpi/env.sh ] && . .mpi/env.sh

CC=${CC:-mpicc}
NP=${NP:-4}
LIMIT=${LIMIT:-8}
TMO="timeout -k 2 $LIMIT"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

$CC -std=gnu11 -O0 example_1.c       -o "$TMP/example_1"        2>/dev/null
$CC -std=gnu11 -O0 example_2.c       -o "$TMP/example_2"        2>/dev/null
$CC -std=gnu11 -O0 example_1_fixed.c -o "$TMP/example_1_fixed" || exit 1
$CC -std=gnu11 -O0 example_2_fixed.c -o "$TMP/example_2_fixed" || exit 1

pass=0 fail=0
check() { # label expected(ok|hang) program
    local label=$1 want=$2 prog=$3 rc verdict
    $TMO mpiexec -n "$NP" "$prog" >/dev/null 2>&1
    rc=$?
    if [ "$want" = hang ]; then
        [ "$rc" -eq 124 ] && verdict=PASS || verdict=FAIL
    else
        [ "$rc" -eq 0 ] && verdict=PASS || verdict=FAIL
    fi
    [ "$verdict" = PASS ] && pass=$((pass + 1)) || fail=$((fail + 1))
    printf '%-30s %-6s %-6s exit=%s\n' "$label" "$want" "$verdict" "$rc"
}

printf '%-30s %-6s %-6s %s\n' "program" "expect" "result" "status"
check "example_1 (original)"  hang "$TMP/example_1"
check "example_2 (original)"  hang "$TMP/example_2"
check "example_1_fixed"       ok   "$TMP/example_1_fixed"
check "example_2_fixed"       ok   "$TMP/example_2_fixed"

printf '\n%d passed, %d failed (ran with %d ranks)\n' "$pass" "$fail" "$NP"
[ "$fail" -eq 0 ]
