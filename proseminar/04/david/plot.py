#!/usr/bin/env python3
"""Plot speedup, efficiency and throughput from benchmark results.

Usage: ./plot.py results_*.csv
"""
import glob
import sys

import matplotlib
import numpy as np

matplotlib.use("Agg")
import matplotlib.pyplot as plt

files = sys.argv[1:] or sorted(glob.glob("results_*.csv"))
data = np.concatenate([np.atleast_1d(np.genfromtxt(f, delimiter=",", names=True, dtype=None,
                                                    encoding=None))
                       for f in files])
column = lambda name: np.atleast_1d(data[name])

N = int(column("N")[0])
ranks = column("ranks").astype(int)
variant = column("variant").astype(str)
Timesteps = column("T").astype(float)
stencil = column("stencil_s").astype(float)

# sequential reference of the same problem size and timestep count
baseline = {t: s for t, s in zip(Timesteps[variant == "seq"], stencil[variant == "seq"])}

styles = [
    ("nonblocking", "o", "-", "non-blocking"),
    ("blocking", "s", "-", "blocking"),
    ("nonblocking_spread", "o", "--", "non-blocking, 8 ranks/node"),
    ("stripes", "^", "-", "non-blocking, 1D stripes"),
    ("nocomm", "x", "", "no exchange (lower bound)"),
]

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10.5, 4))
for name, marker, ls, label in styles:
    sel = (variant == name) & (ranks > 0)
    if not sel.any():
        continue
    base = np.array([baseline.get(t, np.nan) for t in Timesteps[sel]])
    speedup = base / stencil[sel]
    ax1.plot(ranks[sel], speedup, marker=marker, linestyle=ls, label=label)
    ax2.plot(ranks[sel], speedup / ranks[sel], marker=marker, linestyle=ls, label=label)

parallel = np.unique(ranks[ranks > 0])
ax1.plot(parallel, parallel, "k--", label="ideal")
ax1.set_xscale("log", base=2)
ax1.set_yscale("log", base=2)
ax1.set_xlabel("ranks")
ax1.set_ylabel("speedup")
ax1.set_title(f"strong scaling, N={N}")
ax1.legend()
ax1.grid(True, which="both", alpha=0.3)

ax2.axhline(1.0, color="k", linestyle="--", label="ideal")
ax2.set_xscale("log", base=2)
ax2.set_xlabel("ranks")
ax2.set_ylabel("efficiency")
ax2.set_title(f"parallel efficiency, N={N}")
ax2.legend()
ax2.grid(True, which="both", alpha=0.3)

fig.tight_layout()
fig.savefig("scaling.png", dpi=150)

# domain-specific metric: cell updates per second
fig, ax = plt.subplots(figsize=(6, 4))
for name, marker, ls, label in styles:
    sel = variant == name
    if not sel.any():
        continue
    updates = N * N * Timesteps[sel] / stencil[sel]
    ax.plot(ranks[sel], updates / 1e9, marker=marker, linestyle=ls, label=label)
ax.set_xscale("log", base=2)
ax.set_xlabel("ranks")
ax.set_ylabel("cell updates per second [G]")
ax.set_title(f"domain-specific throughput, N={N}")
ax.legend()
ax.grid(True, which="both", alpha=0.3)
fig.tight_layout()
fig.savefig("throughput.png", dpi=150)

for r, t, v, ts, total, ok in zip(ranks, Timesteps, variant, stencil,
                                  column("total_s").astype(float), column("verify").astype(str)):
    speedup = baseline.get(t, np.nan) / ts
    print(f"  T={t:6.0f} ranks={r:3d} {v:12s} stencil={ts:8.3f} s total={total:8.3f} s"
          f" speedup={speedup:6.2f} efficiency={speedup / max(r, 1):5.2f} {ok}")
