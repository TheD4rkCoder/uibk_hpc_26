#!/bin/bash

# Execute job in the partition "lva" unless you have special requirements.
#SBATCH --partition=lva
# Name your job to be able to identify it later
#SBATCH --job-name heat_stencil_2D
# Redirect output stream to this file (append, many jobs share it)
#SBATCH --output=output.log
#SBATCH --open-mode=append
# Enforce exclusive node allocation, do not share with other jobs
#SBATCH --exclusive
#SBATCH --ntasks 1
#SBATCH --time 10:00

set -euo pipefail

if [[ $# -ne 4 ]]; then
	echo "Usage: $0 <naive|tiling|ghost|current> <N> <T> <result_csv>" >&2
	exit 1
fi

variant=$1
N=$2
T=$3
result_csv=$4

# sbatch is called from the heat_stencil_2D directory, the sources and Makefile live there
src_dir=${SLURM_SUBMIT_DIR:-$(pwd)}

build_dir=$(mktemp -d)
stdout_file=$(mktemp)
time_file=$(mktemp)

trap 'rm -rf "$build_dir" "$stdout_file" "$time_file"' EXIT

# compile on the compute node so -march=native targets the benchmark CPU,
# into a private directory so concurrent jobs don't overwrite each other's binaries
program_path=$build_dir/heat_stencil_2D_$variant
make -s -C "$src_dir" BUILD_DIR="$build_dir" "$program_path"

/usr/bin/time -f "%e" -o "$time_file" "$program_path" "$N" "$T" >"$stdout_file" || true
elapsed=$(tail -n 1 "$time_file")
verification=$(sed -n 's/^Verification: //p' "$stdout_file" | head -n 1)

printf '%s,%s,%s,%s,%s,%s\n' "$variant" 0 "$elapsed" "${verification:-MISSING}" "$N" "$T" >> "$result_csv"
