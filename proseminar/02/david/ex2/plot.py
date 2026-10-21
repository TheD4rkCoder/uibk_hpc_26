#!/usr/bin/env python3
"""Usage: python plot_heat.py out/heat_<jobid>.csv
CSV: N,ranks,time_s,verify   (ranks=0 = sequential; garbage lines ignored)
"""
import sys
import io
import re
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

ROW = re.compile(r'^\d+,\d+,[\d.]+,(OK|FAILED)$')


def load(path):
    rows = [l.strip() for l in open(path)
            if ROW.match(l.strip()) and not l.strip().startswith('N,')]
    return pd.read_csv(io.StringIO('\n'.join(rows)),
                       names=['N', 'ranks', 'time_s', 'verify'])


def main(path):
    df = load(path)
    assert (df.verify == 'OK').all(), "some runs FAILED — check before plotting"

    seq = df[df.ranks == 0].groupby('N')['time_s'].mean()
    agg = (df[df.ranks > 0]
           .groupby(['N', 'ranks'])
           .agg(t_mean=('time_s', 'mean'),
                t_std =('time_s', 'std'))
           .reset_index())

    Ns     = sorted(agg.N.unique())
    colors = plt.cm.viridis(np.linspace(0, 0.9, len(Ns)))
    fig, ax = plt.subplots(1, 3, figsize=(16, 5))

    # 1) wall time (dotted = sequential baseline)
    for N, c in zip(Ns, colors):
        s = agg[agg.N == N]
        ax[0].errorbar(s.ranks, s.t_mean, yerr=s.t_std, marker='o',
                       color=c, capsize=3, label=f'N={N}')
        if N in seq.index:
            ax[0].axhline(seq[N], color=c, ls=':', alpha=0.5)
    ax[0].set(xscale='log', yscale='log', xlabel='MPI ranks',
              ylabel='wall time [s]', title='Wall time')

    # 2) speedup vs sequential
    for N, c in zip(Ns, colors):
        if N not in seq.index:
            continue
        s = agg[agg.N == N]
        ax[1].plot(s.ranks, seq[N] / s.t_mean, marker='o',
                   color=c, label=f'N={N}')
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
                   marker='o', color=c, label=f'N={N}')
    ax[2].axhline(100, color='k', ls='--', alpha=0.3)
    ax[2].set(xscale='log', xlabel='MPI ranks',
              ylabel='efficiency [%]', title='Efficiency')

    for a in ax:
        a.grid(True, which='both', alpha=0.3)
        a.legend(fontsize=8)

    plt.tight_layout()
    out = path.replace('.csv', '.png')
    plt.savefig(out, dpi=150)
    print(f"saved {out}  ({len(df)} clean rows)")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(f"usage: {sys.argv[0]} <csv>")
    main(sys.argv[1])