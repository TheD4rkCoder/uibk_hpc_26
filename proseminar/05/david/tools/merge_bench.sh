#!/bin/bash
# Joins the per-group CSVs written by parallel SLURM jobs into results/bench.csv.
set -euo pipefail
cd "$(dirname "$0")/.."
out=results/bench.csv
{
	printf 'solver,ic,n,steps,dt,theta,quad,rep,sim_s,io_s\n'
	for f in results/bench-*.csv; do
		[ -e "$f" ] || continue
		tail -n +2 "$f"
	done
} >"$out"
echo "wrote $out"
