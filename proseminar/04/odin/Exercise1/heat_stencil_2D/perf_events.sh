# perf counters shared by slurm_heat_2D_seq.sh and slurm_heat_2D_mpi.sh (source this file).
#
# lva nodes: 2x Intel Xeon X5650 (Westmere), perf_event_paranoid=2 -> user-space counting only.
# This perf version has no named FP events for Westmere, so FP_COMP_OPS_EXE (event 0x10) is
# programmed raw. It counts *executed* SSE FP uops: a probe loop with exactly 1e8 mul + 1e8 add
# reported 3e8 (scalar and packed alike), so these are raw counter values, not exact FLOPs.
# 7 programmable events on 4 counters -> perf multiplexes and scales the counts.

PERF_COLUMNS="cycles,instructions,fp_scalar,fp_packed,l1d_misses,llc_load_misses,llc_store_misses"
PERF_EVENTS="cycles:u,instructions:u,cpu/event=0x10,umask=0x20,name=fp_scalar/u,cpu/event=0x10,umask=0x10,name=fp_packed/u,L1-dcache-load-misses:u,LLC-load-misses:u,LLC-store-misses:u"

# Sum every event over the given `perf stat -x,` output files (one per MPI rank) and print the
# sums comma-separated in PERF_COLUMNS order; NA for events that are missing or not counted.
perf_sum() {
	# skip missing files (e.g. a rank that crashed before perf wrote its output)
	local files=(/dev/null) file
	for file in "$@"; do
		[[ -r "$file" ]] && files+=("$file")
	done
	awk -F, -v cols="$PERF_COLUMNS" '
		BEGIN {
			n = split(cols, order, ",")
			split("cycles instructions fp_scalar fp_packed L1-dcache-load-misses LLC-load-misses LLC-store-misses", events, " ")
			for (i = 1; i <= n; i++) column[events[i]] = i
		}
		/^#/ || NF < 3 { next }
		{
			event = $3
			sub(/:u$/, "", event)
			if (!(event in column)) next
			if ($1 ~ /^[0-9.]+$/) sum[column[event]] += $1
			else not_counted[column[event]] = 1
		}
		END {
			for (i = 1; i <= n; i++) {
				value = ((i in sum) && !(i in not_counted)) ? sprintf("%.0f", sum[i]) : "NA"
				printf "%s%s", (i > 1 ? "," : ""), value
			}
			printf "\n"
		}
	' "${files[@]}"
}
