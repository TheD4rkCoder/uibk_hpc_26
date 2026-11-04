#!/bin/bash
# Submits the benchmark as one small job per measurement group, so a 1-rank run does not
# hold a 12 node allocation. Usage: ./submit.sh [quick|full]
#
#   quick (default): ranks 1 and 8 with T=2000, plus 96 ranks with the full T=76800
#   full:            every rank count with the full T, in both layouts
#
# Each job writes results_<jobid>.csv; plot.py reads them all:
#   python3 plot.py results_*.csv

module load openmpi/3.1.6-gcc-12.2.0-d2gmn55
make -s || exit 1

T_FULL=76800
if [ "${1:-quick}" = full ]; then
    RUNS="1:$T_FULL 2:$T_FULL 4:$T_FULL 6:$T_FULL 8:$T_FULL 12:$T_FULL 16:$T_FULL \
          24:$T_FULL 32:$T_FULL 48:$T_FULL 96:$T_FULL"
    SPREAD="12 16 24 32 48 96"
else
    RUNS="1:2000 8:2000 96:$T_FULL"
    SPREAD="96"
fi

# one job, with the nodes this decomposition actually needs
submit()
{
    local ranks=$1 per_node=$2 t=$3 layout=$4
    shift 4
    local nodes=$(( (ranks + per_node - 1) / per_node ))
    [ "$nodes" -lt 1 ] && nodes=1
    sbatch --nodes="$nodes" --ntasks=$((nodes * 12)) \
           --job-name="heat_${ranks}${layout}_r" \
           run_one.sh "$t" "$ranks" "$layout" "$@"
}

# sequential reference, one node is enough
submit 0 1 "$T_FULL" packed heat_stencil_2D_seq

for entry in $RUNS; do
    submit "${entry%:*}" 12 "${entry#*:}" packed
done

# same binary, 8 ranks per node: the node is bandwidth bound at 7-8 threads
for ranks in $SPREAD; do
    submit "$ranks" 8 "$T_FULL" spread heat_stencil_2D
done

# the exchange costs at most this much: same decomposition, no communication
submit 96 12 "$T_FULL" packed heat_stencil_2D_nocomm
