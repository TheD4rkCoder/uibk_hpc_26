#!/bin/bash
# Builds once, then submits every benchmark group as its own exclusive SLURM
# job so the whole matrix runs in parallel on quiet nodes.
# Run this on LCC3 from the project directory.
set -euo pipefail
cd "$(dirname "$0")/.."

if type -t module >/dev/null; then
	module load gcc/12.2.0-gcc-8.5.0-p4pe45v
fi

make clean >/dev/null
make -j"$(nproc)" nbody nbody_quad validate validate_quad nbody_gprof >/dev/null

rm -rf results
mkdir -p results

groups=(mandatory steps theta energy validate profile vis)
for n in 1000 2000 5000 10000 20000; do
	groups+=("direct-$n" "bh-$n")
done

ids=()
for group in "${groups[@]}"; do
	id=$(sbatch --parsable --partition=lva --exclusive --cpus-per-task=1 --time=00:30:00 \
		--job-name="nb-$group" --output="results/slurm-$group-%j.out" \
		--wrap="cd '$PWD' && bash tools/benchmark.sh $group")
	ids+=("$id")
done

# Join the per-group CSVs only once every benchmark job succeeded.
dep=$(IFS=:; echo "${ids[*]}")
sbatch --parsable --partition=lva --cpus-per-task=1 --time=00:02:00 \
	--job-name=nb-merge --output=results/slurm-merge-%j.out \
	--dependency="afterany:$dep" --wrap="cd '$PWD' && bash tools/merge_bench.sh" >/dev/null

echo "submitted ${#groups[@]} benchmark jobs plus merge job; watch with: squeue -u \$USER"
