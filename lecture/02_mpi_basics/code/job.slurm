#!/bin/bash
# submission partition
#SBATCH --partition std.q
# name of the job
#SBATCH --job-name my_test_job
# redirect output stream to this file
#SBATCH --output output.dat
# specify parallel environment
#SBATCH --ntasks 8
#SBATCH --ntasks-per-node 2

srun /path/to/application
# or
mpiexec -n $SLURM_NTASKS /path/to/application
