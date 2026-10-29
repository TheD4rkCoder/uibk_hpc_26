#!/usr/bin/env python3

import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import FixedLocator, FormatStrFormatter, NullFormatter, NullLocator
import numpy as np

MPI_NAME = "mpi"
BASELINE_NAME = "ghost"

# chart colors: categorical slots 1/2 (validated pair) + recessive ink for axes and grid
COLOR_MPI = "#2a78d6"
COLOR_SEQ = "#eb6834"
TEXT_PRIMARY = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
MUTED = "#898781"
GRID = "#e1e0d9"
AXIS = "#c3c2b7"
SURFACE = "#fcfcfb"


def parse_csv(csv_path: Path):
    """Group run times by (name, ranks, N, T); rows that did not verify OK are skipped."""
    data = defaultdict(list)
    skipped = []
    with csv_path.open("r", newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            # check verification first: crashed runs may have an empty time field
            if (row.get("verification") or "").strip() != "OK":
                skipped.append(row)
                continue
            try:
                key = (row["name"].strip(), int(row["ranks"]), int(row["N"]), int(row["T"]))
                time = float(row["time"])
            except (KeyError, TypeError, ValueError):
                continue
            data[key].append(time)
    return data, skipped


def summarize_runs(measurements):
    arr = np.asarray(measurements, dtype=float)
    # sample standard deviation over the repetitions (0 if there is only one run)
    std = float(arr.std(ddof=1)) if arr.size > 1 else 0.0
    return {"mean": float(arr.mean()), "std": std, "count": arr.size}


def collect_weak_scaling(data):
    """One point per rank count: MPI time and the sequential baseline at the same N and T."""
    points = []
    for (name, ranks, n, t), times in data.items():
        if name != MPI_NAME:
            continue
        baseline = data.get((BASELINE_NAME, 0, n, t))
        if not baseline:
            print(f"warning: no '{BASELINE_NAME}' baseline for N={n}, T={t} -> {ranks} ranks skipped")
            continue
        mpi = summarize_runs(times)
        seq = summarize_runs(baseline)

        # efficiency = (sequential time / p) / parallel time; uses the baseline at the *same* N,
        # so the small deviations of N^2/p from the target work per rank cancel out
        # scaled speedup = sequential time at this N / parallel time; ideal is p
        speedup = seq["mean"] / mpi["mean"]
        eff = speedup / ranks
        rel_err = np.hypot(seq["std"] / seq["mean"], mpi["std"] / mpi["mean"])
        points.append({
            "ranks": ranks, "N": n, "T": t, "mpi": mpi, "seq": seq,
            "speedup": speedup, "speedup_err": speedup * rel_err,
            "eff": eff, "eff_err": eff * rel_err,
        })
    if not points:
        raise ValueError(f"No '{MPI_NAME}' rows with a matching '{BASELINE_NAME}' baseline found")
    points.sort(key=lambda p: p["ranks"])
    return points


def style_axes(ax):
    ax.set_facecolor(SURFACE)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(AXIS)
    ax.tick_params(colors=TEXT_SECONDARY, labelsize=9)
    ax.grid(True, axis="y", color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)


def label_line_end(ax, x, y, text):
    # direct label at the end of a series, in text ink (the series' own marker carries identity)
    ax.annotate(text, (x, y), xytext=(10, 0), textcoords="offset points",
                va="center", fontsize=9, color=TEXT_PRIMARY)


def plain_log_ticks(ax):
    # readable 1-2-5 ticks on a log axis instead of 10^n labels
    ymin, ymax = ax.get_ylim()
    ticks = [m * 10.0**e for e in range(-3, 5) for m in (1, 2, 5)]
    ax.yaxis.set_major_locator(FixedLocator([t for t in ticks if ymin <= t <= ymax]))
    ax.yaxis.set_major_formatter(FormatStrFormatter("%g"))
    ax.yaxis.set_minor_formatter(NullFormatter())


def make_weak_scaling_plot(points, output_path: Path):
    ranks = np.array([p["ranks"] for p in points], dtype=float)
    mpi_mean = np.array([p["mpi"]["mean"] for p in points])
    mpi_std = np.array([p["mpi"]["std"] for p in points])
    seq_mean = np.array([p["seq"]["mean"] for p in points])
    seq_std = np.array([p["seq"]["std"] for p in points])
    eff = np.array([p["eff"] for p in points])
    eff_err = np.array([p["eff_err"] for p in points])
    reps = sorted({p["mpi"]["count"] for p in points} | {p["seq"]["count"] for p in points})
    t_steps = points[0]["T"]

    fig, (ax_time, ax_eff) = plt.subplots(
        2, 1, figsize=(9, 8), sharex=True, gridspec_kw={"height_ratios": [3, 2]})
    fig.patch.set_facecolor(SURFACE)
    errorbar_style = dict(linewidth=2, markersize=6, capsize=3, elinewidth=1.2)

    # --- runtime: MPI on p ranks vs. the best sequential version at the same N ---
    ax_time.errorbar(ranks, mpi_mean, yerr=mpi_std, fmt="o-", color=COLOR_MPI,
                     label="MPI, p ranks", **errorbar_style)
    ax_time.errorbar(ranks, seq_mean, yerr=seq_std, fmt="s-", color=COLOR_SEQ,
                     label=f"sequential '{BASELINE_NAME}', same N", **errorbar_style)
    ax_time.set_yscale("log")
    ax_time.set_ylabel("Wall time (s, log scale)", color=TEXT_SECONDARY)
    ax_time.set_title(f"2D heat stencil: weak scaling (~6144 cells per rank, T={t_steps})",
                      color=TEXT_PRIMARY, loc="left", fontsize=12)
    label_line_end(ax_time, ranks[-1], mpi_mean[-1], "MPI")
    label_line_end(ax_time, ranks[-1], seq_mean[-1], "sequential")
    plain_log_ticks(ax_time)
    ax_time.legend(frameon=False, fontsize=9, labelcolor=TEXT_PRIMARY, loc="upper left")
    style_axes(ax_time)
    ax_time.grid(True, axis="y", which="minor", color=GRID, linewidth=0.4)

    # --- efficiency: 1.0 = perfect weak scaling ---
    ax_eff.axhline(1.0, color=MUTED, linestyle="--", linewidth=1)
    ax_eff.annotate("ideal", (ranks[0], 1.0), xytext=(0, 4), textcoords="offset points",
                    fontsize=9, color=TEXT_SECONDARY)
    ax_eff.errorbar(ranks, eff, yerr=eff_err, fmt="o-", color=COLOR_MPI, **errorbar_style)
    ax_eff.set_ylim(0, max(1.1, float(np.max(eff + eff_err)) * 1.1))
    ax_eff.set_ylabel("Efficiency  T_seq(N) / (p · T_mpi)", color=TEXT_SECONDARY)
    style_axes(ax_eff)

    # x: log2 rank axis, every measured rank count labeled together with its N
    ax_eff.set_xscale("log", base=2)
    ax_eff.set_xticks(ranks)
    ax_eff.set_xticklabels([f"{int(p['ranks'])}\n{p['N']}" for p in points])
    ax_eff.xaxis.set_minor_locator(NullLocator())
    ax_eff.set_xlim(ranks[0] / 1.2, ranks[-1] * 1.6)  # room for the end labels
    ax_eff.set_xlabel("Ranks p  (problem size N below)", color=TEXT_SECONDARY)

    rep_text = str(reps[0]) if len(reps) == 1 else f"{reps[0]}–{reps[-1]}"
    fig.text(0.01, 0.005, f"Points: mean of {rep_text} repetitions per point; error bars: ±1 sample std "
             f"(efficiency error propagated from both times).", fontsize=8, color=TEXT_SECONDARY)
    fig.tight_layout(rect=(0, 0.02, 1, 1))
    fig.savefig(output_path, dpi=200, facecolor=SURFACE)
    plt.close(fig)


def make_speedup_plot(points, output_path: Path):
    ranks = np.array([p["ranks"] for p in points], dtype=float)
    speedup = np.array([p["speedup"] for p in points])
    speedup_err = np.array([p["speedup_err"] for p in points])
    reps = sorted({p["mpi"]["count"] for p in points} | {p["seq"]["count"] for p in points})

    fig, ax = plt.subplots(figsize=(9, 6))
    fig.patch.set_facecolor(SURFACE)

    # ideal weak scaling: p ranks do p times the work in the same time
    ax.plot(ranks, ranks, color=MUTED, linestyle="--", linewidth=1)
    label_line_end(ax, ranks[-1], ranks[-1], "ideal (S = p)")
    ax.errorbar(ranks, speedup, yerr=speedup_err, fmt="o-", color=COLOR_MPI,
                linewidth=2, markersize=6, capsize=3, elinewidth=1.2)
    label_line_end(ax, ranks[-1], speedup[-1], f"MPI  (S = {speedup[-1]:.1f})")

    ax.set_title(f"2D heat stencil: scaled speedup, weak scaling (T={points[0]['T']})",
                 color=TEXT_PRIMARY, loc="left", fontsize=12)
    ax.set_yscale("log", base=2)
    ax.set_ylabel("Speedup  T_seq(N) / T_mpi(p)  (log scale)", color=TEXT_SECONDARY)
    style_axes(ax)
    ax.grid(True, axis="y", which="minor", color=GRID, linewidth=0.4)

    # y: plain numbers on powers of two and the rank counts that are measured
    ymin = min(float(np.min(speedup - speedup_err)), 1.0) / 1.3
    ax.set_ylim(ymin, ranks[-1] * 1.3)
    ax.yaxis.set_major_locator(FixedLocator([2.0**k for k in range(-6, 8)]))
    ax.yaxis.set_major_formatter(FormatStrFormatter("%g"))
    ax.yaxis.set_minor_formatter(NullFormatter())

    ax.set_xscale("log", base=2)
    ax.set_xticks(ranks)
    ax.set_xticklabels([f"{int(p['ranks'])}\n{p['N']}" for p in points])
    ax.xaxis.set_minor_locator(NullLocator())
    ax.set_xlim(ranks[0] / 1.2, ranks[-1] * 2.6)  # room for the end labels
    ax.set_xlabel("Ranks p  (problem size N below)", color=TEXT_SECONDARY)

    rep_text = str(reps[0]) if len(reps) == 1 else f"{reps[0]}–{reps[-1]}"
    fig.text(0.01, 0.005, f"Points: mean of {rep_text} repetitions per point; error bars: ±1 sample std "
             f"propagated from both times. Baseline: sequential '{BASELINE_NAME}' at the same N.",
             fontsize=8, color=TEXT_SECONDARY)
    fig.tight_layout(rect=(0, 0.02, 1, 1))
    fig.savefig(output_path, dpi=200, facecolor=SURFACE)
    plt.close(fig)


def make_table(points, output_path: Path):
    base_mpi = points[0]["mpi"]["mean"]
    lines = [
        "| Ranks | N | MPI mean (s) | MPI std (s) | Seq mean (s) | Seq std (s) | Runs (mpi/seq) "
        "| Speedup | Efficiency | T_mpi(1)/T_mpi(p) |",
        "|------:|--:|-------------:|------------:|-------------:|------------:|---------------:"
        "|--------:|-----------:|------------------:|",
    ]
    for p in points:
        lines.append(
            f"| {p['ranks']} | {p['N']} | {p['mpi']['mean']:.3f} | {p['mpi']['std']:.3f} "
            f"| {p['seq']['mean']:.3f} | {p['seq']['std']:.3f} "
            f"| {p['mpi']['count']}/{p['seq']['count']} "
            f"| {p['speedup']:.2f} ± {p['speedup_err']:.2f} "
            f"| {p['eff']:.2f} ± {p['eff_err']:.2f} | {base_mpi / p['mpi']['mean']:.2f} |"
        )
    table = f"### 2D heat stencil weak scaling (T={points[0]['T']})\n\n" + "\n".join(lines) + "\n"
    output_path.write_text(table, encoding="utf-8")
    return table


def main():
    parser = argparse.ArgumentParser(description="Plot the 2D heat stencil weak-scaling results")
    parser.add_argument("--csv", type=Path, default=Path(__file__).with_name("output.csv"),
                        help="Path to the benchmark CSV")
    parser.add_argument("--output-dir", type=Path, default=Path(__file__).resolve().parent,
                        help="Directory for the generated plot and table")
    args = parser.parse_args()

    csv_path = args.csv.resolve()
    if not csv_path.exists():
        raise FileNotFoundError(f"Benchmark CSV not found: {csv_path}")

    data, skipped = parse_csv(csv_path)
    for row in skipped:
        print(f"warning: skipped run that did not verify: {dict(row)}")

    points = collect_weak_scaling(data)

    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    plot_path = output_dir / "heat_2D_weak_scaling.png"
    speedup_path = output_dir / "heat_2D_weak_scaling_speedup.png"
    table_path = output_dir / "heat_2D_weak_scaling.md"

    make_weak_scaling_plot(points, plot_path)
    make_speedup_plot(points, speedup_path)
    table = make_table(points, table_path)

    print(f"Saved {plot_path}")
    print(f"Saved {speedup_path}")
    print(f"Saved {table_path}")
    print()
    print(table)


if __name__ == "__main__":
    main()
