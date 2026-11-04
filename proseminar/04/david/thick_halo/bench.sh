#!/bin/bash
#SBATCH --job-name=thick
#SBATCH --nodes=8
#SBATCH --ntasks=96
#SBATCH --cpus-per-task=1
#SBATCH --time=00:20:00
#SBATCH --partition=lva
#SBATCH --output=thick_%j.log
#SBATCH --exclusive

module load openmpi/3.1.6-gcc-12.2.0-d2gmn55
make -s || exit 1

# correctness: a thick halo must give exactly the same result as the 1-row one
# (n=64 puts the source cell on a rank boundary at 4 and 8 ranks, n=101 does not)
for n in 64 101; do
    for ranks in 1 4 8; do
        mpirun --map-by ppr:12:node -np $ranks ./thick_stripe1 $n 64 base.bin > /dev/null
        for halo in 2 4 8; do
            mpirun --map-by ppr:12:node -np $ranks ./thick_stripe$halo $n 64 thick.bin > /dev/null
            cmp -s base.bin thick.bin && echo "n=$n halo $halo, $ranks ranks: identical to halo 1"
        done
    done
done
rm -f base.bin thick.bin

# warm up (freshly allocated nodes are slower)
for halo in 1 2 4 8; do
    mpirun --map-by ppr:12:node -np 12 ./thick_stripe$halo 768 1000 > /dev/null
done

echo "n,steps,ranks,halo,time_s,verify"
for pass in 1 2 3; do
    for halo in 1 2 4 8; do
        for ranks in 12 24 48 96; do
            mpirun --map-by ppr:12:node -np "$ranks" ./thick_stripe$halo 768 76800
        done
    done
done
