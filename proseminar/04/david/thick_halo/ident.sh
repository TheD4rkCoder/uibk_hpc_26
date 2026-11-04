#!/bin/bash
#SBATCH --job-name=thickident
#SBATCH --nodes=1
#SBATCH --ntasks=8
#SBATCH --time=00:05:00
#SBATCH --partition=lva
#SBATCH --output=ident_%j.log

module load openmpi/3.1.6-gcc-12.2.0-d2gmn55
make -s || exit 1
for n in 64 101; do
    for ranks in 1 4 8; do
        mpirun -np $ranks ./thick_stripe1 $n 64 base.bin > /dev/null
        for halo in 2 4 8; do
            mpirun -np $ranks ./thick_stripe$halo $n 64 thick.bin > /dev/null
            if cmp -s base.bin thick.bin; then
                echo "n=$n halo $halo, $ranks ranks: identical to halo 1"
            else
                echo "n=$n halo $halo, $ranks ranks: DIFFERS"
            fi
        done
    done
done
rm -f base.bin thick.bin
