#!/usr/bin/env bash
# Build and run the minimal reproductions for the bugs found in
# example_1.c / example_2.c.  "hang" = the timeout fired (exit 124).
set -u
cd "$(dirname "$0")/.."
[ -f .mpi/env.sh ] && . .mpi/env.sh

CC=${CC:-mpicc}
TMO="timeout -k 2 6"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

$CC -std=gnu11 -O0 tests/tag_mismatch.c         -o "$OUT/tag_mismatch"        || exit 1
$CC -std=gnu11 -O0 tests/peer_expr.c            -o "$OUT/peer_expr"           || exit 1
$CC -std=gnu11 -O0 tests/type_count_mismatch.c  -o "$OUT/type_count_mismatch" || exit 1
$CC -std=gnu11 -O0 tests/sendrecv_types.c       -o "$OUT/sendrecv_types"      || exit 1
$CC -std=gnu11 -O0 tests/selfsend_large.c       -o "$OUT/selfsend_large"      || exit 1
$CC -std=gnu11 -O0 tests/ring_blocking.c        -o "$OUT/ring_blocking"       || exit 1
$CC -std=gnu11 -O0 -DUSE_ISEND tests/ring_blocking.c -o "$OUT/ring_isend"    || exit 1

pass=0 fail=0
run() { # label expected(ok|hang|abort) ranks binary
    local label=$1 want=$2 n=$3 prog=$4 rc verdict
    $TMO mpiexec -n "$n" "$prog" >/dev/null 2>&1
    rc=$?
    case $want in
    ok)    [ "$rc" -eq 0 ] && verdict=PASS || verdict=FAIL ;;
    hang)  [ "$rc" -eq 124 ] && verdict=PASS || verdict=FAIL ;;
    abort) [ "$rc" -ne 0 ] && [ "$rc" -ne 124 ] && verdict=PASS || verdict=FAIL ;;
    esac
    [ "$verdict" = PASS ] && pass=$((pass + 1)) || fail=$((fail + 1))
    printf '%-28s %-5s %-6s %-5s exit=%s\n' "$label" "$want" "$verdict" "$n" "$rc"
}

printf '%-28s %-5s %-6s %-5s %s\n' "test" "want" "result" "ranks" "status"
run "tag_mismatch"          hang   2 "$OUT/tag_mismatch"
run "type_count_mismatch"   abort  2 "$OUT/type_count_mismatch"
run "sendrecv_types"        ok     4 "$OUT/sendrecv_types"
run "selfsend_large"        hang   1 "$OUT/selfsend_large"
run "ring_blocking 64 MiB"  hang   2 "$OUT/ring_blocking"
run "ring_isend 64 MiB"     ok     2 "$OUT/ring_isend"

echo
echo "--- peer expression: intended vs compile-time result ---"
"$OUT/peer_expr"

printf '\n%d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]
