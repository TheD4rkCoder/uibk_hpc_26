#!/bin/bash
# Runs one group of the benchmark suite:
#   mandatory steps direct-1000 ... direct-20000 bh-1000 ... bh-20000
#   theta energy validate profile vis
#   all (everything sequentially, for a single-machine run)
# tools/submit_bench.sh submits every group as its own exclusive SLURM job.
set -euo pipefail
cd "$(dirname "$0")/.."

GROUP=${1:-all}
BENCH="results/bench-$GROUP.csv"
ENERGY=results/energy.csv
mkdir -p results

# Every group owns one CSV: parallel SLURM jobs never write the same file
# (flock on the NFS home hangs). tools/merge_bench.sh joins them afterwards.
printf 'solver,ic,n,steps,dt,theta,quad,rep,sim_s,io_s\n' >"$BENCH"
if [ "$GROUP" = energy ] || [ "$GROUP" = all ]; then
	printf 'solver,ic,n,dt,step,energy\n' >"$ENERGY"
fi

# run_bench BIN SOLVER IC N STEPS DT THETA [REPS]
run_bench() {
	local bin=$1 solver=$2 ic=$3 n=$4 steps=$5 dt=$6 theta=$7 reps=${8:-3}
	local line sim io quad rep row
	for rep in $(seq 1 "$reps"); do
		line=$("$bin" --n "$n" --steps "$steps" --dt "$dt" --solver "$solver" --ic "$ic" \
			--theta "$theta" --quiet)
		sim=$(sed -n 's/.*sim_s=\([0-9.]*\).*/\1/p' <<<"$line")
		io=$(sed -n 's/.*io_s=\([0-9.]*\).*/\1/p' <<<"$line")
		quad=$(sed -n 's/.*quad=\([0-9]\).*/\1/p' <<<"$line")
		row="$solver,$ic,$n,$steps,$dt,$theta,$quad,$rep,$sim,$io"
		printf '%s\n' "$row" >>"$BENCH"
	done
}

# energy_run BIN SOLVER N STEPS DT EVERY
energy_run() {
	local bin=$1 solver=$2 n=$3 steps=$4 dt=$5 every=$6
	local row
	"$bin" --n "$n" --steps "$steps" --dt "$dt" --softening 0.2 --energy-every "$every" \
		--solver "$solver" --ic plummer --quiet 2>&1 |
		awk -v s="$solver" -v n="$n" -v dt="$dt" \
			'/^step / { print s ",plummer," n "," dt "," $2 "," $4 }' |
		while read -r row; do
			printf '%s\n' "$row" >>"$ENERGY"
		done
}

case "$GROUP" in
mandatory)
	run_bench ./nbody direct cube 5000 100 1 0.5 3
	;;
steps)
	for steps in 10 20 50; do
		run_bench ./nbody direct cube 5000 "$steps" 1 0.5 2
	done
	;;
direct-*)
	n=${GROUP#direct-}
	run_bench ./nbody direct cube "$n" 20 1 0.5 3
	;;
bh-*)
	n=${GROUP#bh-}
	run_bench ./nbody bh cube "$n" 20 1 0.5 3
	run_bench ./nbody_quad bh cube "$n" 20 1 0.5 3
	;;
theta)
	for theta in 0.2 0.3 0.5 0.7 1.0 2.0 100.0; do
		run_bench ./nbody bh cube 5000 20 1 "$theta" 3
		run_bench ./nbody_quad bh cube 5000 20 1 "$theta" 3
	done
	;;
energy)
	for dt in 1 0.5 0.25 0.1; do
		steps=$(awk -v dt="$dt" 'BEGIN { printf "%d", 50 / dt }')
		every=$(awk -v s="$steps" 'BEGIN { printf "%d", (s >= 10) ? s / 10 : 1 }')
		energy_run ./nbody bh 2000 "$steps" "$dt" "$every"
	done
	energy_run ./nbody direct 2000 500 0.1 100
	energy_run ./nbody_quad bh 2000 500 0.1 100
	;;
validate)
	./validate >results/validate.txt 2>&1
	./validate_quad >results/validate_quad.txt 2>&1
	;;
profile)
	perf stat -e task-clock,cycles,instructions,cache-references,cache-misses,branch-misses \
		-o results/perf_direct.txt ./nbody --n 5000 --steps 20 --solver direct --quiet \
		>/dev/null 2>&1 || true
	perf stat -e task-clock,cycles,instructions,cache-references,cache-misses,branch-misses \
		-o results/perf_bh.txt ./nbody --n 5000 --steps 20 --solver bh --quiet \
		>/dev/null 2>&1 || true
	perf stat -e task-clock,cycles,instructions,cache-references,cache-misses,branch-misses \
		-o results/perf_bh_quad.txt ./nbody_quad --n 5000 --steps 20 --solver bh --quiet \
		>/dev/null 2>&1 || true
	perf record -F 199 -g -o results/perf_bh.data ./nbody --n 5000 --steps 20 --solver bh \
		--quiet >/dev/null 2>&1 || true
	perf report --stdio -i results/perf_bh.data >results/perf_bh_report.txt 2>/dev/null || true
	perf record -F 199 -g -o results/perf_direct.data ./nbody --n 5000 --steps 20 --solver direct \
		--quiet >/dev/null 2>&1 || true
	perf report --stdio -i results/perf_direct.data >results/perf_direct_report.txt 2>/dev/null || true
	./nbody_gprof --n 5000 --steps 20 --solver bh --quiet >/dev/null
	gprof ./nbody_gprof gmon.out >results/gprof_bh.txt
	;;
vis)
	./nbody --solver bh --n 4000 --steps 300 --ic clusters --theta 0.5 \
		--output results/clusters.dat --quiet
	gnuplot -e "input='results/clusters.dat'; output='results/output.gif'; frames=300; delay=6; range=130; pointsize=0.4" \
		tools/animate.plt
	;;
all)
	bash tools/benchmark.sh mandatory
	bash tools/benchmark.sh steps
	for n in 1000 2000 5000 10000 20000; do
		bash tools/benchmark.sh "direct-$n"
		bash tools/benchmark.sh "bh-$n"
	done
	bash tools/benchmark.sh theta
	bash tools/benchmark.sh energy
	bash tools/benchmark.sh validate
	bash tools/benchmark.sh profile
	bash tools/benchmark.sh vis
	bash tools/merge_bench.sh
	;;
*)
	echo "unknown group: $GROUP" >&2
	exit 1
	;;
esac
