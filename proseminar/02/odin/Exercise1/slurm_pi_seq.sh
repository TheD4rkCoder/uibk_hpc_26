#!/bin/bash

set -euo pipefail

# Execute job in the partition "lva" unless you have special requirements.
#SBATCH --partition=lva
# Name your job to be able to identify it later
#SBATCH --job-name mc_pi
# Redirect output stream to this file
#SBATCH --output=output.log
# Enforce exclusive node allocation, do not share with other jobs
#SBATCH --exclusive
#SBATCH --ntasks 1
#SBATCH --time 2:00

if [[ $# -ne 2 ]]; then
	echo "Usage: $0 <samples> <result_csv>" >&2
	exit 1
fi


program_path=/scratch/cb761131/uibk_hpc_26/proseminar/02/odin/Exercise1/pi_seq
samples=$1
result_csv=$2
stdout_file=$(mktemp)
time_file=$(mktemp)

trap 'rm -f "$stdout_file" "$time_file"' EXIT

/usr/bin/time -f "%e" -o "$time_file" "$program_path" "$samples" >"$stdout_file"
elapsed=$(<"$time_file")
pi_estimate=$(sed -n 's/.*= //p' "$stdout_file" | tail -n 1)

printf '%s,%s,%s,%s,%s\n' "pi_seq" 0 "$elapsed" "$pi_estimate" "$samples" >> "$result_csv"
