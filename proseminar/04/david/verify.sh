#!/bin/bash
#SBATCH --job-name=heat_2D_verify
#SBATCH --partition=lva
#SBATCH --nodes=2
#SBATCH --ntasks=24
#SBATCH --exclusive
#SBATCH --time=00:10:00

# Usage: sbatch verify.sh [N] [T]
# Compares the sequential result with the MPI results, bit for bit: the per cell
# expression is the same in both programs (shared kernel in heat_stencil_2D.h), so
# even in single precision the results must be identical.

set -e
module load openmpi/3.1.6-gcc-12.2.0-d2gmn55
make

N=${1:-257}
T=${2:-200}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

run_mpi()
{
    local ranks=$1 bin=$2 n=$3 t=$4 out=$5
    local nodes=$(( (ranks + 11) / 12 ))
    local ppr=$(( (ranks + nodes - 1) / nodes ))
    rm -f "$out" # a stale dump would make cmp compare the wrong file
    mpiexec -n "$ranks" --map-by "ppr:$ppr:node" ./"$bin" "$n" "$t" -q -d "$out" > /dev/null
}

check()
{
    local n=$1 t=$2 ranks=$3
    for precision in "" _float; do
        ./heat_stencil_2D_seq$precision "$n" "$t" -q -d "$tmp/seq.bin" > /dev/null
        for variant in "" _blocking; do
            run_mpi "$ranks" "heat_stencil_2D$precision$variant" "$n" "$t" "$tmp/mpi.bin"
            cmp "$tmp/seq.bin" "$tmp/mpi.bin" ||
                { echo "FAIL heat_stencil_2D$precision$variant N=$n ranks=$ranks"; exit 1; }
        done
    done
    echo "N=$n ranks=$ranks: bitwise identical (double and float)"
}

# both rank grids that divide N evenly and ones that do not
for ranks in 1 2 3 4 6 8 12 16; do
    check "$N" "$T" "$ranks"
done
for ranks in 4 8 12; do
    check 19 100 "$ranks"
done
echo "all checks passed"
