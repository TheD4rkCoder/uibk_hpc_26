# 2D heat stencil — results

N=768, T=76800, isolated walls, source at (N/4, N/4),
`-O3 -march=native`, every point is the fastest of three runs after a short warm-up run
(five runs for the `nocomm` point). Ranks are spread evenly over the nodes they get
(`--map-by ppr:N:node`), so 16 ranks means 8 + 8.

`./submit.sh full` submits one job per measurement group (the small groups of ranks get few
nodes, the 96 rank groups get as many as they need), each writing `results_<jobid>.csv`;
`plot.py` reads all of them. Times are the stencil loop in seconds, the CSVs also have
the whole-program time (start-up plus the final gather); `results.csv` is all groups merged.

| ranks | non-blocking | blocking | 1D stripes | single precision | non-blocking, 8 ranks/node |
|------:|-------------:|---------:|-----------:|-----------------:|---------------------------:|
| 0 (seq) | 60.88 |  |  |  |  |
| 1 | 60.69 | 65.98 | 61.00 | 35.12 |  |
| 2 | 33.54 | 34.68 | 33.70 | 17.96 |  |
| 4 | 16.61 | 17.53 | 16.11 | 9.56 |  |
| 6 | 11.68 | 12.17 | 11.46 | 6.56 |  |
| 8 | 10.08 | 10.13 | 10.07 | 5.46 |  |
| 12 | 10.53 | 10.31 | 10.57 | 4.98 | 6.22 |
| 16 | 5.51 | 5.75 | 5.51 | 2.97 | 5.43 |
| 24 | 5.24 | 5.17 | 5.68 | 1.90 | 3.71 |
| 32 | 3.41 | 3.55 | 3.93 | 1.44 | 2.72 |
| 48 | 1.97 | 2.28 | 2.51 | 1.13 | 1.72 |
| 96 | 1.07 | 1.60 | 2.23 | 1.19 | 1.12 |

Figures: `scaling.png` (speedup, efficiency), `throughput.png` (cell updates per second).
Reproduce with `./submit.sh full` (or `quick`), verify with `sbatch verify.sh`, plot with
`python3 plot.py results.csv`.


- 96 ranks: 1.12 s = 54.4x (57 % efficiency) when 8 ranks go on each of 12 nodes, and
  1.07 s = 57.0x (59 %) when 12 ranks fill 8 nodes — but that packed point bounced between
  1.07 s and 1.80 s over the repeats, so the 8-ranks-per-node number is the one to trust.
  Single precision: 1.19 s at 96 ranks, 35.12 s sequential. Without any exchange
  (`-DNOCOMM`, wrong results, lower bound) 0.70 s = 86.6x.
- memory bound:
  - One node saturates at 7-8 of its 12 threads: 8 ranks need 10.08 s, 12 ranks on one node
  10.53 s, so the four extra ranks buy nothing. The stencil is memory bound (two 4.7 MB
  grids streamed per step), not compute bound.
- The packed layout at 96 ranks has high variance: 1.07-1.80 s over the repeats, while
  the 8-ranks-per-node layout stayed at 1.06-1.19 s
- 1D stripes are within noise up to 16 ranks (one neighbour pair, half the messages) and
  lose from 24 ranks on, where a stripe halo is two full rows of 768 values instead of a
  ~64-value frame.
- Single precision is 1.73x faster sequentially and 1.75x at 48 ranks: half the bytes per
  cell update, so the bandwidth bound moves. At 96 ranks the gain disappears (1.19 s against 1.07s, no clue why)
- the arithmetic is identical in all builds (bitwise result comparison to make sure)

## Ranks per node

Default OpenMPI mapping (`--map-by socket`) fills nodes one at a time: 16 ranks became
12 + 4 (checked with `hostname`) and took 8.02 s instead of 5.47 s with 8 + 8,
because all blocks have the same size and the 12 ranks on the crowded node set the pace.
`run_one.sh` therefore always passes `--map-by ppr:N:node`.

| ranks | packed (12 per node) | spread (8 per node) |
|------:|---------------------:|--------------------:|
| 12 | 10.53 (1 node) | 6.22 (2 nodes) |
| 16 | 5.51 (2 nodes) | 5.43 (2 nodes) |
| 24 | 5.24 (2 nodes) | 3.71 (3 nodes) |
| 32 | 3.41 (3 nodes) | 2.72 (4 nodes) |
| 48 | 1.97 (4 nodes) | 1.72 (6 nodes) |
| 96 | 1.07 (8 nodes) | 1.12 (12 nodes) |

Leaving cores idle and using more nodes wins everywhere that a node would otherwise be
filled to 12 ranks, and costs nothing at 16 ranks, where both layouts are 8 + 8. The only
exception is the packed 96 rank point, which is the unstable one.

## Metrics

- Domain-specific: **cell updates per second** = N^2 * T / time, 744 M/s sequential to
  42.4 G/s at 96 ranks (`throughput.png`).
- Domain-unspecific: **speedup** T_seq / T_p and **efficiency** speedup / p
  (`scaling.png`).

## Problems seen

- everything is SSE and memory bound
- Interference on the nodes only ever makes a run slower (up to 30 % within a single job),
  so `run_one.sh` repeats each measurement (`REPS=3`) and keeps the fastest one. Single
  measurements of the same configuration were off by more than the differences we discuss.
- The first run on freshly allocated nodes is sometimes up to 60 % slower than the next one
  on the same nodes (1.70 s then 1.08 s), so `run_one.sh` does a throwaway T=1000 run
  before each measurement (lessened the variance).
- The packed 96 rank point stays unstable even with the warm-up run and the repeats
  [1.07 s-1.80 s].
- Forcing 12 ranks per node inside an allocation with 8 slots per node
  (`--oversubscribe`) puts two ranks on one slot and gives 1.4-1.9 s instead of 1.1 s.
  That is how I first "measured" the spread layout being 25 % faster, before the repeats.

## Correctness

`heat_stencil_2D_seq -d file` and the MPI version (rank 0 gathers and writes the same
format) are compared with `cmp`: bitwise identical for N=257, T=200 on 1, 2, 3, 4, 6, 8,
12 and 16 ranks (double and single precision, both exchange variants) and for N=19 on
4, 8 and 12 ranks. Every run additionally checks the physical range 273 K <= T <= 333 K.
