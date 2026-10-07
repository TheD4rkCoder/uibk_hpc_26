#!/bin/bash

# Execute job in the partition "lva" unless you have special requirements.
#SBATCH --partition=lva
# Name your job to be able to identify it later
#SBATCH --job-name task1
# Redirect output stream to this file
#SBATCH --output=output.log
# Maximum number of tasks (=processes) to start per node
#SBATCH --ntasks-per-node=2
# Enforce exclusive node allocation, do not share with other jobs
#SBATCH --exclusive
# Allocate 2 Nodes for the job
#SBATCH --nodes=2

iterations=$1

module load openmpi/3.1.6-gcc-12.2.0-d2gmn55

rm -rf results
mkdir results

latency="/scratch/cb761131/uibk_hpc_26/proseminar/01/odin/osu-micro-benchmarks-5.8/mpi/pt2pt/osu_latency"
bandwidth="/scratch/cb761131/uibk_hpc_26/proseminar/01/odin/osu-micro-benchmarks-5.8/mpi/pt2pt/osu_bw"

for i in $(seq 1 $iterations)
do
    # Same node, same socket
    mpiexec -n 2 --map-by ppr:2:socket --bind-to core --report-bindings $latency >> results/latency_output_same_socket.log
    mpiexec -n 2 --map-by ppr:2:socket --bind-to core --report-bindings $bandwidth >> results/bandwidth_output_same_socket.log


    # Same node, different sockets
    mpiexec -n 2 --map-by ppr:1:socket --bind-to core --report-bindings $latency >> results/latency_output_diff_socket.log
    mpiexec -n 2 --map-by ppr:1:socket --bind-to core --report-bindings $bandwidth >> results/bandwidth_output_diff_socket.log

    # Two nodes
    mpiexec -n 2 --map-by ppr:1:node --bind-to core --report-bindings $latency >> results/latency_output_two_nodes.log
    mpiexec -n 2 --map-by ppr:1:node --bind-to core --report-bindings $bandwidth >> results/bandwidth_output_two_nodes.log
done
