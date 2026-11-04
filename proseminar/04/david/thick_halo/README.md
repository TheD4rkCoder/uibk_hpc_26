# thick halo stripes (quick and dirty)

CLARIFICATION: done by deepseek v4.1 flash

`thick_stripe.c` is a throwaway 1D-stripe version with a `HALO` row deep halo
that is exchanged once every `HALO` steps instead of every step
(`-DHALO=1,2,4,8`).  The halo rows are recomputed with a shrinking overlap: at
sub-step `s` the ghost depth below `HALO-s` is still valid, so rows
`1-(HALO-s) .. rows+(HALO-s)` are computed.  That costs
`~(HALO-1)/rows` extra cell updates per step and saves `HALO` times the
messages (same bytes).  Two details keep the result bitwise identical to the
1-row version: wall ranks mirror the recomputed halo rows instead of
computing them, and a recomputed halo row holding the source cell gets clamped
like the owner does.

    sbatch bench.sh      # identity checks, then ranks 12/24/48/96 x 3 passes

Timings (`thick_406738.csv`, N=768, T=76800, packed 12 ranks/node, median of 3):

| halo | 12 | 24 | 48 | 96 | vs halo 1 (48/96) |
|-----:|---:|---:|---:|---:|------------------:|
| 1 | 10.42 | 5.82 | 3.23 | 2.32 | - |
| 2 | 10.58 | 5.84 | 3.02 | 2.17 | -6.5 % / -6.5 % |
| 4 | 10.86 | 6.11 | 3.20 | 2.38 | -0.9 % / +2.5 % |
| 8 | 11.51 | 6.80 | 3.90 | 2.84 | +20.6 % / +22.4 % |

So a 2 row halo is worth ~6 % at 48-96 ranks and deeper halos are worse: with
8-16 rows per rank the recomputed halo rows cost more than the exchange they
save.  For comparison the halo exchange in the normal code is ~0.45 s of the
2.3 s at 96 ranks, so there is not much to win in the first place.
