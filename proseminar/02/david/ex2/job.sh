#!/bin/bash
#SBATCH --partition=lva
#SBATCH --exclusive
#SBATCH --output=out/heat_%j.out
#SBATCH --ntasks=96
#SBATCH --time=00:30:00

module load openmpi/3.1.6-gcc-12.2.0-d2gmn55

make heat_stencil_1D_seq heat_stencil_1D_mpi

mkdir -p out
csv=out/heat_${SLURM_JOB_ID}.csv
# ranks=0 is the sequential version
echo "N,ranks,time_s,verify" > "$csv"

N_VALUES=(512 1024 2048 4096 6144)     # T = N*500, so cost ~ N^2
RANKS=(1 2 4 8 16 32 48 96)
REPEATS=3

# for N in "${N_VALUES[@]}"; do
    # warmup
    # ./heat_stencil_1D_seq $N -q >/dev/null

    # for i in $(seq 1 $REPEATS); do
    #     ./heat_stencil_1D_seq $N -q >> "$csv"
    # done

    ranks=96
    N=6144
    # for ranks in "${RANKS[@]}"; do
        # for i in $(seq 1 $REPEATS); do
            mpiexec -n $ranks ./heat_stencil_1D_mpi $N -q >> "$csv" # 2>/dev/null
            # mpiexec -n $ranks ./heat_stencil_1D_mpi $N -q >> "$csv"
        # done
    # done
# done

make clean