#!/bin/bash

# Execute job in the partition "lva" unless you have special requirements.
#SBATCH --partition=lva
# Name your job to be able to identify it later
#SBATCH --job-name heat_stencil
# Redirect output stream to this file
#SBATCH --output=output.log
# Enforce exclusive node allocation, do not share with other jobs
#SBATCH --exclusive
#SBATCH --time 10:00

set -euo pipefail

if [[ $# -ne 2 ]]; then
	echo "Usage: $0 <N> <result_csv>" >&2
	exit 1
fi


module load openmpi/3.1.6-gcc-12.2.0-d2gmn55

program_path=/scratch/cb761131/uibk_hpc_26/proseminar/02/odin/Exercise2/heat_stencil_1D/heat_stencil_1D_mpi
N=$1
result_csv=$2
stdout_file=$(mktemp)
time_file=$(mktemp)

trap 'rm -f "$stdout_file" "$time_file"' EXIT

/usr/bin/time -f "%e" -o "$time_file" mpiexec -n "$SLURM_NTASKS" --report-bindings "$program_path" "$N" >"$stdout_file" || true
elapsed=$(tail -n 1 "$time_file")
verification=$(sed -n 's/^Verification: //p' "$stdout_file" | head -n 1)

printf '%s,%s,%s,%s,%s\n' "heat_mpi" "$SLURM_NTASKS" "$elapsed" "${verification:-MISSING}" "$N" >> "$result_csv"
