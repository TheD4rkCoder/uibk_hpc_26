#!/bin/bash

#SBATCH --partition=lva
#SBATCH --job-name test
#SBATCH --output=output.log
#SBATCH --nodes=2
#SBATCH --ntasks-per-node=2
# for all tests I essentially need 4: 2 nodes with 2 tasks=rank=processes per node
# #SBATCH --ntasks=2 
# do not share nodes with other jobs
#SBATCH --exclusive

module load openmpi/3.1.6-gcc-12.2.0-d2gmn55
# mpiexec -n $SLURM_NTASKS /bin/hostname

# module load hwloc
# mpiexec -n $SLURM_NTASKS lstopo --of txt


# ./configure CC=mpicc CXX=mpic++
# make -j % -j for parallel build
# mpiexec -n $SLURM_NTASKS ./osu-micro-benchmarks-5.8/mpi/pt2pt/osu_bw
# mpiexec -n $SLURM_NTASKS ./osu-micro-benchmarks-5.8/mpi/pt2pt/osu_latency


repeats=5

num_benchs=3
benchs=("SameSocket" "DiffSocket" "DiffNode")
bench_flags=(
    "--map-by core --bind-to core" # different cores same socket
    "--map-by socket --bind-to core" # different sockets same node
    "--map-by node --bind-to core" # different node
)

mkdir "tmp"
header="Size"
files=""

for idx in $(seq 0 $((num_benchs-1))); do
    name="${benchs[$idx]}"
    flags="${bench_flags[$idx]}"

    for b in $(seq 1 $repeats); do
        # CLARIFICATION: the extraction logic has been done using gemini flash 3.8
        # grep -v '^#' only give lines that don't start with # (comments)
        mpiexec -n 2 --report-bindings $flags ./osu-micro-benchmarks-5.8/mpi/pt2pt/osu_bw | grep -v '^#' > "tmp/${name}_${b}.raw"
        # awk takes each line and prints the second token (e.g. Bandwidth (MB/s))
        awk '{print $2}' "tmp/${name}_${b}.raw" > "tmp/${name}_${b}.val"
        files="$files tmp/${name}_${b}.val"
        header="${header},${name}_Run${b}"
    done
done

# take message sizes from the first generated file
awk '{print $1}' "tmp/${benchs[0]}_1.raw" > "tmp/sizes.val"

echo "$header" > bandwidth.csv
# `paste` merges lines of files, and -d the delimiter to use (here a comma) to combine them
paste -d, "tmp/sizes.val" $files >> "bandwidth.csv"
rm -rf "tmp"



mkdir "tmp"
header="Size" 
files=""

for idx in $(seq 0 $((num_benchs-1))); do
    name="${benchs[$idx]}"
    flags="${bench_flags[$idx]}"

    for b in $(seq 1 $repeats); do
        # CLARIFICATION: the extraction logic has been done using gemini flash 3.8
        # grep -v '^#' only give lines that don't start with # (comments)
        mpiexec -n 2 --report-bindings $flags ./osu-micro-benchmarks-5.8/mpi/pt2pt/osu_latency | grep -v '^#' > "tmp/${name}_${b}.raw"
        # awk takes each line and prints the second token (e.g. Bandwidth (MB/s))
        awk '{print $2}' "tmp/${name}_${b}.raw" > "tmp/${name}_${b}.val"
        files="$files tmp/${name}_${b}.val"
        header="${header},${name}_Run${b}"
    done
done

# take message sizes from the first generated file
awk '{print $1}' "tmp/${benchs[0]}_1.raw" > "tmp/sizes.val"

echo "$header" > latency.csv
# `paste` merges lines of files, and -d the delimiter to use (here a comma) to combine them
paste -d, "tmp/sizes.val" $files >> "latency.csv"
rm -rf "tmp"