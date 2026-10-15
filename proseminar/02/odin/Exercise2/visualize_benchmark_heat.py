#!/usr/bin/env python3

import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import FixedLocator, NullFormatter, NullLocator, ScalarFormatter
import numpy as np


def parse_csv(csv_path: Path):
    data = defaultdict(list)
    with csv_path.open("r", newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            try:
                name = row["name"].strip()
                ranks = int(row["ranks"])
                time = float(row["time"])
                n = int(row["N"])
            except (KeyError, TypeError, ValueError):
                continue
            data[(name, ranks, n)].append(time)
    return data


def summarize_runs(measurements):
    arr = np.asarray(measurements, dtype=float)
    mean = float(arr.mean())
    std = float(arr.std(ddof=0)) if arr.size > 1 else 0.0
    return {"mean": mean, "std": std, "count": arr.size}




def apply_log_axes(ax, x_values=None):
    """Log y axis with plain-number ticks; optional base-2 log x axis."""
    # --- y: log10, with readable 1-2-5 ticks instead of 10^n labels ---
    ax.set_yscale("log")
    ymin, ymax = ax.get_ylim()
    ax.set_ylim(ymin * 0.85, ymax * 1.2)  # a bit of headroom for error bars
    ymin, ymax = ax.get_ylim()
    ticks = [m * 10.0**e for e in range(-3, 4) for m in (1, 2, 5)]
    ticks = [t for t in ticks if ymin <= t <= ymax]
    ax.yaxis.set_major_locator(FixedLocator(ticks))
    ax.yaxis.set_major_formatter(ScalarFormatter())
    ax.yaxis.set_minor_formatter(NullFormatter())  # minor ticks/grid, no labels
    ax.grid(True, which="both", linestyle="--", alpha=0.25)

    # --- x: log base 2, powers of two plus the last entry (e.g. 96) ---
    if x_values is not None:
        xmax = max(x_values)
        xticks = [2**k for k in range(0, int(np.floor(np.log2(xmax))) + 1)]
        if xmax not in xticks:
            xticks.append(int(xmax))
        ax.set_xscale("log", base=2)
        ax.set_xticks(xticks)
        ax.set_xticklabels([str(t) for t in xticks])
        ax.xaxis.set_minor_locator(NullLocator())
        ax.set_xlim(min(xticks) / 1.1, xmax * 1.1)

def make_rank_vs_time_plot(data, target_n, output_path: Path):
    mpi_runs = {}
    for (name, ranks, n), times in data.items():
        if name == "heat_mpi" and n == target_n:
            mpi_runs[ranks] = summarize_runs(times)

    seq_runs = {}
    for (name, ranks, n), times in data.items():
        if name == "heat_seq" and n == target_n:
            seq_runs[ranks] = summarize_runs(times)

    if not mpi_runs:
        raise ValueError(f"No MPI timings found for N={target_n} in the CSV")
    if not seq_runs:
        raise ValueError(f"No sequential timings found for N={target_n} in the CSV")

    seq_baseline = next(iter(seq_runs.values()))
    seq_mean = seq_baseline["mean"]
    seq_std = seq_baseline["std"]

    ranks = sorted(mpi_runs)
    means = [mpi_runs[r]["mean"] for r in ranks]
    stds = [mpi_runs[r]["std"] for r in ranks]

    fig, ax = plt.subplots(figsize=(8, 5))
    x_positions = np.asarray(ranks, dtype=float)
    yerr = np.asarray(stds, dtype=float) if any(std > 0 for std in stds) else None

    ax.errorbar(
        x_positions,
        means,
        yerr=yerr,
        fmt="o-",
        linewidth=2,
        markersize=6,
        capsize=4,
        color="tab:blue",
        label="MPI runs",
    )
    ax.axhline(
        seq_mean,
        color="tab:orange",
        linestyle="--",
        linewidth=2,
        label=f"Sequential baseline (N={target_n})",
    )

    if seq_std > 0:
        ax.axhspan(
            seq_mean - seq_std,
            seq_mean + seq_std,
            color="tab:orange",
            alpha=0.15,
            zorder=0,
        )

    ax.set_title(f"Heat stencil runtime for N={target_n}")
    ax.set_xlabel("Number of ranks")
    ax.set_ylabel("Time (s) logscale")
    apply_log_axes(ax, x_values=ranks)
    ax.legend()
    fig.tight_layout()
    fig.savefig(output_path, dpi=200)
    plt.close(fig)


def make_seq_vs_96_plot(data, output_path: Path):
    ns = sorted({n for (_, _, n) in data.keys() if n is not None})
    seq_means = []
    seq_stds = []
    mpi_means = []
    mpi_stds = []

    for n in ns:
        seq_times = data.get(("heat_seq", 0, n), [])
        if not seq_times:
            continue
        seq_summary = summarize_runs(seq_times)
        seq_means.append(seq_summary["mean"])
        seq_stds.append(seq_summary["std"])

        mpi_times = data.get(("heat_mpi", 96, n), [])
        if not mpi_times:
            continue
        mpi_summary = summarize_runs(mpi_times)
        mpi_means.append(mpi_summary["mean"])
        mpi_stds.append(mpi_summary["std"])

    if not seq_means or not mpi_means:
        raise ValueError("No sequential and/or 96-rank timings found for the requested comparison")

    common_ns = []
    seq_plot = []
    seq_err = []
    mpi_plot = []
    mpi_err = []

    for n in sorted(set(ns) & set({n for n in ns if ("heat_seq", 0, n) in data and ("heat_mpi", 96, n) in data})):
        common_ns.append(n)
        seq_plot.append(summarize_runs(data[("heat_seq", 0, n)])["mean"])
        seq_err.append(summarize_runs(data[("heat_seq", 0, n)])["std"])
        mpi_plot.append(summarize_runs(data[("heat_mpi", 96, n)])["mean"])
        mpi_err.append(summarize_runs(data[("heat_mpi", 96, n)])["std"])

    if not common_ns:
        raise ValueError("No common N values found between sequential and 96-rank runs")

    fig, ax = plt.subplots(figsize=(8, 5))
    seq_yerr = np.asarray(seq_err, dtype=float) if any(err > 0 for err in seq_err) else None
    mpi_yerr = np.asarray(mpi_err, dtype=float) if any(err > 0 for err in mpi_err) else None

    ax.errorbar(
        common_ns,
        seq_plot,
        yerr=seq_yerr,
        fmt="o-",
        linewidth=2,
        markersize=6,
        capsize=4,
        color="tab:green",
        label="Sequential",
    )
    ax.errorbar(
        common_ns,
        mpi_plot,
        yerr=mpi_yerr,
        fmt="s-",
        linewidth=2,
        markersize=6,
        capsize=4,
        color="tab:red",
        label="96 ranks",
    )

    ax.set_title("Sequential vs 96-rank heat stencil runtime")
    ax.set_xlabel("Problem size N")
    ax.set_ylabel("Time (s)")
    apply_log_axes(ax)
    ax.legend()
    fig.tight_layout()
    fig.savefig(output_path, dpi=200)
    plt.close(fig)


def make_speedup_table(data, target_n, output_path: Path):
    seq_times = [t for (name, _, n), times in data.items() if name == "heat_seq" and n == target_n for t in times]
    if not seq_times:
        raise ValueError(f"No sequential timings found for N={target_n} in the CSV")
    seq = summarize_runs(seq_times)

    mpi_runs = {
        ranks: summarize_runs(times)
        for (name, ranks, n), times in data.items()
        if name == "heat_mpi" and n == target_n
    }
    if not mpi_runs:
        raise ValueError(f"No MPI timings found for N={target_n} in the CSV")

    lines = [
        "| Ranks | Mean time (s) | Std (s) | Runs | Speedup |",
        "|------:|--------------:|--------:|-----:|--------:|",
        f"| sequential | {seq['mean']:.3f} | {seq['std']:.3f} | {seq['count']} | 1.00 |",
    ]
    for ranks in sorted(mpi_runs):
        run = mpi_runs[ranks]
        speedup = seq["mean"] / run["mean"]
        lines.append(f"| {ranks} | {run['mean']:.3f} | {run['std']:.3f} | {run['count']} | {speedup:.2f} |")

    table = f"### Heat stencil speedup for N={target_n}\n\n" + "\n".join(lines) + "\n"
    output_path.write_text(table, encoding="utf-8")
    return table


def main():
    parser = argparse.ArgumentParser(description="Visualize the heat stencil benchmark CSV results")
    parser.add_argument("--csv", type=Path, default=Path(__file__).with_name("result.csv"), help="Path to the benchmark CSV")
    parser.add_argument("--output-dir", type=Path, default=Path(__file__).resolve().parent, help="Directory for generated plots")
    parser.add_argument("--target-n", type=int, default=6144, help="Problem size to highlight in the first plot")
    args = parser.parse_args()

    csv_path = args.csv.resolve()
    if not csv_path.exists():
        raise FileNotFoundError(f"Benchmark CSV not found: {csv_path}")

    data = parse_csv(csv_path)
    if not data:
        raise ValueError(f"No benchmark rows found in {csv_path}")

    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    make_rank_vs_time_plot(data, args.target_n, output_dir / f"heat_runtime_vs_ranks_N{args.target_n}.png")
    make_seq_vs_96_plot(data, output_dir / "heat_runtime_seq_vs_96ranks.png")
    table = make_speedup_table(data, args.target_n, output_dir / f"heat_speedup_N{args.target_n}.md")

    print(f"Saved plots to {output_dir}")
    print(f"- {output_dir / f'heat_runtime_vs_ranks_N{args.target_n}.png'}")
    print(f"- {output_dir / 'heat_runtime_seq_vs_96ranks.png'}")
    print(f"- {output_dir / f'heat_speedup_N{args.target_n}.md'}")
    print()
    print(table)


if __name__ == "__main__":
    main()
