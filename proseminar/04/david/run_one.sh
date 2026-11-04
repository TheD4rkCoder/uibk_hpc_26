#!/bin/bash
#SBATCH --partition=lva
#SBATCH --exclusive
#SBATCH --time=00:45:00
#SBATCH --output=bench_%j.log

# One measurement group: every binary in one decomposition, or the sequential reference.
# Usage: run_one.sh <T> <ranks> <packed|spread> [binary ...]
# The node count comes from Slurm, so submit.sh decides how much of the partition a
# group needs; the ranks are then spread evenly over those nodes.

module load openmpi/3.1.6-gcc-12.2.0-d2gmn55

T=$1
ranks=$2
layout=$3
shift 3
BINS=${*:-heat_stencil_2D heat_stencil_2D_blocking heat_stencil_2D_stripes heat_stencil_2D_float}

# every run is repeated and the fastest one is kept: interference on the node only
# ever makes a run slower, so the minimum is the cleanest estimate of the real cost
REPS=${REPS:-3}

N=768
out="results_${SLURM_JOB_ID:-local}.csv"
echo "N,T,ranks,variant,stencil_s,total_s,verify" > "$out"

variant_name()
{
    case "$1" in
        heat_stencil_2D_seq)      echo seq ;;
        heat_stencil_2D)          echo nonblocking ;;
        heat_stencil_2D_blocking) echo blocking ;;
        heat_stencil_2D_stripes)  echo stripes ;;
        heat_stencil_2D_float)    echo float ;;
        heat_stencil_2D_nocomm)   echo nocomm ;;
    esac
}

run_once()
{
    if [ "$ranks" -eq 0 ]; then
        ./"$1" "$N" "$T" -q
    else
        mpiexec -n "$ranks" --map-by "ppr:$ppr:node" ./"$1" "$N" "$T" -q |
            grep -v libibcm | tail -n 1
    fi
}

# small warm-up run first: on freshly allocated nodes the first run is up to 60 % slower
warmup()
{
    if [ "$ranks" -eq 0 ]; then
        ./"$1" "$N" 1000 -q > /dev/null
    else
        mpiexec -n "$ranks" --map-by "ppr:$ppr:node" ./"$1" "$N" 1000 -q > /dev/null 2>&1
    fi
}

for bin in $BINS; do
    nodes=$SLURM_NNODES
    ppr=$(( (ranks + nodes - 1) / nodes ))
    [ "$ranks" -eq 0 ] && ppr=1

    warmup "$bin"
    line=$(for rep in $(seq "$REPS"); do run_once "$bin"; done | sort -t, -k3 -g | head -n 1)

    variant=$(variant_name "$bin")
    [ "$layout" = spread ] && variant="${variant}_spread"
    echo "$N,$T,$ranks,$variant,$(echo "$line" | cut -d, -f3),$(echo "$line" | cut -d, -f4),$(echo "$line" | cut -d, -f5)" >> "$out"
    cat "$out" | tail -n 1
done
