#!/bin/bash

# Execute job in the partition "lva" unless you have special requirements.
#SBATCH --partition=lva
# Name your job to be able to identify it later
#SBATCH --job-name heat_stencil_2D_mpi
# Redirect output stream to this file (append, many jobs share it)
#SBATCH --output=output.log
#SBATCH --open-mode=append
# Enforce exclusive node allocation, do not share with other jobs
#SBATCH --exclusive
#SBATCH --time 10:00

set -euo pipefail

if [[ $# -ne 3 ]]; then
	echo "Usage: sbatch --ntasks <ranks> $0 <N> <T> <result_csv>" >&2
	exit 1
fi

N=$1
T=$2
result_csv=$3

module load openmpi/3.1.6-gcc-12.2.0-d2gmn55

# sbatch is called from the heat_stencil_2D directory, the sources and Makefile live there
src_dir=${SLURM_SUBMIT_DIR:-$(pwd)}

build_dir=$(mktemp -d)
stdout_file=$(mktemp)
time_file=$(mktemp)

trap 'rm -rf "$build_dir" "$stdout_file" "$time_file"' EXIT

# compile on the compute node so -march=native targets the benchmark CPU,
# into a private directory so concurrent jobs don't overwrite each other's binaries
program_path=$build_dir/heat_stencil_2D_mpi
make -s -C "$src_dir" BUILD_DIR="$build_dir" "$program_path"

/usr/bin/time -f "%e" -o "$time_file" mpiexec -n "$SLURM_NTASKS" --report-bindings "$program_path" "$N" "$T" >"$stdout_file" || true
elapsed=$(tail -n 1 "$time_file")
verification=$(sed -n 's/^Verification: //p' "$stdout_file" | head -n 1)

printf '%s,%s,%s,%s,%s\n' "mpi" "$SLURM_NTASKS" "$elapsed" "${verification:-MISSING}" "$N" >> "$result_csv"
