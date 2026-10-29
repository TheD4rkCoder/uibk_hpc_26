#!/bin/bash

# Execute job in the partition "lva" unless you have special requirements.
#SBATCH --partition=lva
# Name your job to be able to identify it later
#SBATCH --job-name heat_blocking
# Redirect output stream to this file (append, many jobs share it)
#SBATCH --output=output.log
#SBATCH --open-mode=append
# Enforce exclusive node allocation, do not share with other jobs
#SBATCH --exclusive
#SBATCH --time 10:00

set -euo pipefail

if [[ $# -ne 5 ]]; then
	echo "Usage: sbatch --ntasks <ranks> $0 <1D|2D> <blocking|non_blocking> <N> <T> <result_csv>" >&2
	exit 1
fi

dim=$1
variant=$2
N=$3
T=$4
result_csv=$5

module load openmpi/3.1.6-gcc-12.2.0-d2gmn55

# sbatch is called from the blocking_vs_non_blocking directory, the stencils live next to it
bench_dir=${SLURM_SUBMIT_DIR:-$(pwd)}

case "$dim/$variant" in
	1D/blocking)     src_dir=$bench_dir/../heat_stencil_1D; target=heat_stencil_1D_bench_blocking ;;
	1D/non_blocking) src_dir=$bench_dir/../heat_stencil_1D; target=heat_stencil_1D_bench_non_blocking ;;
	2D/blocking)     src_dir=$bench_dir/../heat_stencil_2D; target=heat_stencil_2D_mpi ;;
	2D/non_blocking) src_dir=$bench_dir/../heat_stencil_2D; target=heat_stencil_2D_mpi_non_blocking ;;
	*) echo "Unknown configuration: $dim/$variant" >&2; exit 1 ;;
esac

# the binary must be on the shared filesystem: with more ranks than one node has cores,
# mpiexec starts it on other nodes too, which can't see this node's /tmp
build_dir=$bench_dir/build_$SLURM_JOB_ID
mkdir -p "$build_dir"
stdout_file=$(mktemp)
time_file=$(mktemp)

trap 'rm -rf "$build_dir" "$stdout_file" "$time_file"' EXIT

# compile on the compute node so -march=native targets the benchmark CPU,
# into a per-job directory so concurrent jobs don't overwrite each other's binaries
program_path=$build_dir/$target
make -s -C "$src_dir" BUILD_DIR="$build_dir" "$program_path"

/usr/bin/time -f "%e" -o "$time_file" mpiexec -n "$SLURM_NTASKS" --report-bindings "$program_path" "$N" "$T" >"$stdout_file" || true
elapsed=$(tail -n 1 "$time_file")
verification=$(sed -n 's/^Verification: //p' "$stdout_file" | head -n 1)

printf '%s,%s,%s,%s,%s,%s,%s\n' "$dim" "$variant" "$SLURM_NTASKS" "$elapsed" "${verification:-MISSING}" "$N" "$T" >> "$result_csv"
