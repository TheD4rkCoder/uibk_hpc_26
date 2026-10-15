#!/bin/bash

# Execute job in the partition "lva" unless you have special requirements.
#SBATCH --partition=lva
# Name your job to be able to identify it later
#SBATCH --job-name heat_stencil
# Redirect output stream to this file
#SBATCH --output=output.log
# Enforce exclusive node allocation, do not share with other jobs
#SBATCH --exclusive
#SBATCH --ntasks 1
#SBATCH --time 10:00

set -euo pipefail

if [[ $# -ne 2 ]]; then
	echo "Usage: $0 <N> <result_csv>" >&2
	exit 1
fi


program_path=/scratch/cb761131/uibk_hpc_26/proseminar/02/odin/Exercise2/heat_stencil_1D/heat_stencil_1D_seq
N=$1
result_csv=$2
stdout_file=$(mktemp)
time_file=$(mktemp)

trap 'rm -f "$stdout_file" "$time_file"' EXIT

/usr/bin/time -f "%e" -o "$time_file" "$program_path" "$N" >"$stdout_file" || true
elapsed=$(tail -n 1 "$time_file")
verification=$(sed -n 's/^Verification: //p' "$stdout_file" | head -n 1)

printf '%s,%s,%s,%s,%s\n' "heat_seq" 0 "$elapsed" "${verification:-MISSING}" "$N" >> "$result_csv"
