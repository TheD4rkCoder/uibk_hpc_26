#!/bin/bash

#SBATCH --partition=lva
#SBATCH --exclusive
#SBATCH --output=out/pi_%j.out
#SBATCH --ntasks=1
#SBATCH --time=00:05:00

# #SBATCH --cpus-per-task=1
# #SBATCH --nodes=1
# #SBATCH --ntasks-per-node=1


module load openmpi/3.1.6-gcc-12.2.0-d2gmn55

make pi_seq

mkdir -p out
csv=out/pi_${SLURM_JOB_ID}.csv
# ranks=0 is the sequential version
echo "N,ranks,pi,time_s" > "$csv"

N_VALUES=(10000000 33300000 100000000 333000000 1000000000)   # 10^7, 10^8, 10^9
REPEATS=3

for N in "${N_VALUES[@]}"; do
    for i in $(seq 1 $REPEATS); do
        mpiexec -n 1 ./pi_seq $N >> "$csv"
    done
done

make clean