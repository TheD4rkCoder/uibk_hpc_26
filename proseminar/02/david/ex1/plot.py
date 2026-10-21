#!/usr/bin/env python3
"""Usage: python plot_pi.py out/pi_results_<jobid>.csv
CSV: N,ranks,pi,time_s   (ranks=0 = sequential baseline)
"""
import sys
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


def main(path):
    df  = pd.read_csv(path)
    seq = df[df.ranks == 0].set_index('N')['time_s']
    df  = df[df.ranks > 0]

    agg = (df.groupby(['N', 'ranks'])
             .agg(t_mean=('time_s', 'mean'),
                  t_std =('time_s', 'std'))
             .reset_index())

    Ns     = sorted(agg.N.unique())
    colors = plt.cm.viridis(np.linspace(0, 0.9, len(Ns)))
    fig, ax = plt.subplots(1, 3, figsize=(16, 5))

    # 1) wall time, dotted line = sequential baseline
    for N, c in zip(Ns, colors):
        s = agg[agg.N == N]
        ax[0].errorbar(s.ranks, s.t_mean, yerr=s.t_std, marker='o',
                       color=c, capsize=3, label=f'N={N:.0e}')
        if N in seq.index:
            ax[0].axhline(seq[N], color=c, ls=':', alpha=0.5)
    ax[0].set(xscale='log', yscale='log', xlabel='MPI ranks',
              ylabel='wall time [s]', title='Wall time')

    # 2) speedup vs sequential
    for N, c in zip(Ns, colors):
        if N not in seq.index:
            continue
        s = agg[agg.N == N]
        ax[1].plot(s.ranks, seq[N] / s.t_mean, marker='o', color=c, label=f'N={N:.0e}')
    ranks = sorted(agg.ranks.unique())
    ax[1].plot(ranks, ranks, 'k--', alpha=0.3, label='ideal')
    ax[1].set(xscale='log', yscale='log', xlabel='MPI ranks',
              ylabel='T_seq / T_par', title='Strong scaling')

    # 3) parallel efficiency
    for N, c in zip(Ns, colors):
        if N not in seq.index:
            continue
        s = agg[agg.N == N]
        ax[2].plot(s.ranks, seq[N] / (s.t_mean * s.ranks) * 100,
                   marker='o', color=c, label=f'N={N:.0e}')
    ax[2].axhline(100, color='k', ls='--', alpha=0.3)
    ax[2].set(xscale='log', xlabel='MPI ranks',
              ylabel='efficiency [%]', title='Efficiency')

    for a in ax:
        a.grid(True, which='both', alpha=0.3)
        a.legend(fontsize=8)

    plt.tight_layout()
    out = path.replace('.csv', '.png')
    plt.savefig(out, dpi=150)
    print(f"saved {out}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(f"usage: {sys.argv[0]} <csv>")
    main(sys.argv[1])