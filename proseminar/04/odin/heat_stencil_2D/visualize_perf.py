#!/usr/bin/env python3

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import FormatStrFormatter, NullFormatter, NullLocator
import numpy as np

from visualize_weak_scaling import (
    AXIS, GRID, MUTED, SURFACE, TEXT_PRIMARY, TEXT_SECONDARY, style_axes, summarize_runs)

# --- machine model: lva node = 2x Intel Xeon X5650 (Westmere), datasheet values ---
CLOCK_GHZ = 2.67
CORES_PER_NODE = 12
FLOP_PER_CYCLE = 4            # SSE: one 2-wide double add + one 2-wide double mul per cycle
L2_BYTES_PER_CYCLE = 32       # L2 -> L1 fill bandwidth per core
NODE_DRAM_GBS = 64.0          # 2 sockets x 3 channels DDR3-1333 (10.67 GB/s each)

# FLOPs per cell update as written in the source:
#   tc + 0.2 * (tl + tr + tu + td + (-4 * tc))  ->  5 adds + 2 muls
FLOP_PER_CELL = 7
CACHE_LINE = 64

SEQ_VARIANTS = ["naive", "tiling", "ghost", "current"]
MPI_NAME = "mpi"
# categorical slots 1-5 (validated as a set); markers as secondary encoding
STYLE = {
    "naive": ("#2a78d6", "o"),
    "tiling": ("#eb6834", "s"),
    "ghost": ("#1baf7a", "^"),
    "current": ("#eda100", "D"),
    MPI_NAME: ("#e87ba4", "v"),
}
PERF_FIELDS = ["cycles", "instructions", "fp_scalar", "fp_packed",
               "l1d_misses", "llc_load_misses", "llc_store_misses"]


def parse_csv(csv_path: Path):
    """Per-run derived metrics grouped by (name, ranks, N, T); only verified runs with perf data."""
    runs = defaultdict(list)
    skipped = 0
    with csv_path.open("r", newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            if (row.get("verification") or "").strip() != "OK":
                skipped += 1
                continue
            try:
                name, ranks = row["name"].strip(), int(row["ranks"])
                n, t, time = int(row["N"]), int(row["T"]), float(row["time"])
                counters = {f: float(row[f]) for f in PERF_FIELDS}
            except (KeyError, TypeError, ValueError):
                # older rows without perf columns, or a counter reported as NA
                skipped += 1
                continue
            runs[(name, ranks, n, t)].append(derive_metrics(n, t, time, counters))
    return runs, skipped


def derive_metrics(n, t, time, c):
    cells = n * n * t
    flops = FLOP_PER_CELL * cells
    llc_misses = c["llc_load_misses"] + c["llc_store_misses"]
    return {
        "time": time,
        "cells_per_s": cells / time,
        "gflops": flops / time / 1e9,
        "l1_miss_per_s": c["l1d_misses"] / time,
        "llc_miss_per_s": llc_misses / time,
        # arithmetic intensity w.r.t. the traffic each level has to serve
        "ai_l2": flops / (c["l1d_misses"] * CACHE_LINE) if c["l1d_misses"] else math.inf,
        "ai_dram": flops / (llc_misses * CACHE_LINE) if llc_misses else math.inf,
        # executed SSE FP uops (scalar + 2 x packed) relative to the analytic FLOP count
        "fp_counter_ratio": (c["fp_scalar"] + 2 * c["fp_packed"]) / flops,
    }


def summarize(runs):
    summary = {}
    for key, metrics in runs.items():
        summary[key] = {}
        for m in metrics[0]:
            values = [r[m] for r in metrics]
            if all(math.isfinite(v) for v in values):
                summary[key][m] = summarize_runs(values)
            else:
                # e.g. no LLC misses at all -> intensity w.r.t. DRAM is unbounded
                summary[key][m] = {"mean": math.inf, "std": 0.0, "count": len(values)}
        summary[key]["count"] = len(metrics)
    return summary


def finish(fig, output_path: Path, footnote: str):
    fig.text(0.01, 0.005, footnote, fontsize=8, color=TEXT_SECONDARY)
    fig.tight_layout(rect=(0, 0.05 if "\n" in footnote else 0.03, 1, 1))
    fig.savefig(output_path, dpi=200, facecolor=SURFACE)
    plt.close(fig)


def seq_entries(summary):
    """Sequential sweep runs (T = N*100), as {variant: {N: stats}}."""
    entries = defaultdict(dict)
    for (name, _, n, t), stats in summary.items():
        if name in SEQ_VARIANTS and t == n * 100:
            entries[name][n] = stats
    return entries


def mpi_entries(summary):
    points = [(ranks, n, stats) for (name, ranks, n, _), stats in summary.items() if name == MPI_NAME]
    return sorted(points)


def bar_panel(ax, entries, metric, scale, ylabel, title):
    sizes = sorted({n for per_n in entries.values() for n in per_n})
    variants = [v for v in SEQ_VARIANTS if v in entries]
    width = 0.8 / max(len(variants), 1)
    for k, variant in enumerate(variants):
        color, _ = STYLE[variant]
        xs = [i + (k - (len(variants) - 1) / 2) * width for i in range(len(sizes))]
        means = [entries[variant][n][metric]["mean"] / scale if n in entries[variant] else np.nan for n in sizes]
        stds = [entries[variant][n][metric]["std"] / scale if n in entries[variant] else 0 for n in sizes]
        # surface-colored edge = 2px gap between adjacent bars
        ax.bar(xs, means, width, yerr=stds, color=color, edgecolor=SURFACE, linewidth=1.5,
               label=variant, error_kw=dict(ecolor=TEXT_SECONDARY, elinewidth=1, capsize=2))
    ax.set_xticks(range(len(sizes)))
    ax.set_xticklabels([f"N={n}" for n in sizes])
    ax.set_ylabel(ylabel, color=TEXT_SECONDARY)
    ax.set_title(title, color=TEXT_PRIMARY, loc="left", fontsize=11)
    ax.legend(frameon=False, fontsize=8, ncol=2, labelcolor=TEXT_PRIMARY)
    style_axes(ax)


def mpi_panel(ax, points, metric, scale, ylabel, title, ideal=False):
    color, marker = STYLE[MPI_NAME]
    ranks = np.array([p[0] for p in points], dtype=float)
    means = np.array([p[2][metric]["mean"] for p in points]) / scale
    stds = np.array([p[2][metric]["std"] for p in points]) / scale
    if ideal:
        # weak scaling ideal: throughput grows linearly with the rank count
        ax.plot(ranks, means[0] * ranks / ranks[0], color=MUTED, linestyle="--", linewidth=1)
        ax.annotate("ideal", (ranks[-1], means[0] * ranks[-1] / ranks[0]), xytext=(-30, 4),
                    textcoords="offset points", fontsize=8, color=TEXT_SECONDARY)
        ax.set_yscale("log")
        ax.yaxis.set_major_formatter(FormatStrFormatter("%g"))
        ax.yaxis.set_minor_formatter(NullFormatter())
    ax.errorbar(ranks, means, yerr=stds, fmt=marker + "-", color=color, linewidth=2,
                markersize=6, capsize=3, elinewidth=1.2)
    ax.set_xscale("log", base=2)
    ax.set_xticks(ranks)
    ax.set_xticklabels([str(int(r)) for r in ranks], fontsize=8)
    ax.xaxis.set_minor_locator(NullLocator())
    ax.set_xlabel("Ranks p (weak scaling)", color=TEXT_SECONDARY)
    ax.set_ylabel(ylabel, color=TEXT_SECONDARY)
    ax.set_title(title, color=TEXT_PRIMARY, loc="left", fontsize=11)
    style_axes(ax)


def make_throughput_plot(summary, output_path: Path):
    entries, points = seq_entries(summary), mpi_entries(summary)
    fig, (ax_seq, ax_mpi) = plt.subplots(1, 2, figsize=(12, 4.8))
    fig.patch.set_facecolor(SURFACE)
    if entries:
        bar_panel(ax_seq, entries, "cells_per_s", 1e6, "Mcells/s", "Sequential variants (T = N·100)")
    if points:
        mpi_panel(ax_mpi, points, "cells_per_s", 1e6, "Mcells/s, all ranks (log scale)",
                  "MPI, total cell updates per second", ideal=True)
    finish(fig, output_path, f"Cell updates per second = N²·T / wall time. "
           f"GFLOP/s = {FLOP_PER_CELL} × cells/s. Mean ± 1 sample std over repetitions.")


def make_cache_plot(summary, output_path: Path):
    entries, points = seq_entries(summary), mpi_entries(summary)
    fig, axes = plt.subplots(2, 2, figsize=(12, 8))
    fig.patch.set_facecolor(SURFACE)
    for row, (metric, label) in enumerate([("l1_miss_per_s", "L1d load misses"),
                                           ("llc_miss_per_s", "LLC load + store misses")]):
        if entries:
            bar_panel(axes[row][0], entries, metric, 1e6, f"{label} (M/s)", f"Sequential: {label} per second")
        if points:
            mpi_panel(axes[row][1], points, metric, 1e6, f"{label} (M/s, all ranks)",
                      f"MPI: {label} per second")
    finish(fig, output_path, "perf stat user-space counters (multiplexed and scaled), summed over all ranks. "
           "Mean ± 1 sample std over repetitions.")


def make_roofline_plot(summary, output_path: Path):
    peak = CLOCK_GHZ * FLOP_PER_CYCLE
    l2_bw = CLOCK_GHZ * L2_BYTES_PER_CYCLE
    dram_bw = NODE_DRAM_GBS / CORES_PER_NODE

    fig, ax = plt.subplots(figsize=(9, 6.5))
    fig.patch.set_facecolor(SURFACE)

    x_range, y_range = (0.03, 30), (0.3, peak * 2.5)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlim(*x_range)
    ax.set_ylim(*y_range)
    fig.tight_layout(rect=(0, 0.05, 1, 1))  # final axes geometry, needed for the label angle

    ai = np.logspace(np.log10(x_range[0]), np.log10(x_range[1]), 400)
    # memory roofs, labeled along their slope where no data points are
    for bw, label, label_ai in [(l2_bw, f"L2 → L1  {l2_bw:.0f} GB/s", 0.035),
                                (dram_bw, f"DRAM (1/12 of node)  {dram_bw:.1f} GB/s", 0.8)]:
        ax.plot(ai, np.minimum(peak, ai * bw), color=MUTED, linewidth=1.2)
        p0, p1 = ax.transData.transform([(label_ai, label_ai * bw), (label_ai * 2, label_ai * 2 * bw)])
        angle = math.degrees(math.atan2(p1[1] - p0[1], p1[0] - p0[0]))
        ax.annotate(label, (label_ai, label_ai * bw), xytext=(0, 5), textcoords="offset points",
                    rotation=angle, rotation_mode="anchor", fontsize=8, color=TEXT_SECONDARY)
    ax.annotate(f"peak {peak:.1f} GFLOP/s per core (SSE, no AVX)", (x_range[1], peak), xytext=(-4, 5),
                textcoords="offset points", ha="right", fontsize=8, color=TEXT_SECONDARY)

    for name in SEQ_VARIANTS + [MPI_NAME]:
        keys = sorted(k for k in summary if k[0] == name)
        if name in SEQ_VARIANTS:
            keys = [k for k in keys if k[3] == k[2] * 100]
        pts = [(summary[k]["ai_l2"]["mean"], summary[k]["gflops"]["mean"] / max(k[1], 1)) for k in keys]
        pts = [p for p in pts if math.isfinite(p[0])]
        if not pts:
            continue
        color, marker = STYLE[name]
        label = f"{name} (N = {', '.join(str(k[2]) for k in keys)})" if name != MPI_NAME else "mpi (per core, 1–96 ranks)"
        # surface-colored ring keeps overlapping markers apart
        ax.scatter(*zip(*pts), color=color, marker=marker, s=80, edgecolor=SURFACE, linewidth=1,
                   zorder=3, label=label)

    for axis in (ax.xaxis, ax.yaxis):
        axis.set_major_formatter(FormatStrFormatter("%g"))
        axis.set_minor_formatter(NullFormatter())
    ax.set_xlabel("Arithmetic intensity (FLOP / byte of L1-miss traffic, log scale)", color=TEXT_SECONDARY)
    ax.set_ylabel("GFLOP/s per core (log scale)", color=TEXT_SECONDARY)
    ax.set_title("2D heat stencil: roofline (Intel Xeon X5650, per core)",
                 color=TEXT_PRIMARY, loc="left", fontsize=12)
    ax.legend(frameon=False, fontsize=8, labelcolor=TEXT_PRIMARY, loc="lower right")
    style_axes(ax)
    finish(fig, output_path, f"FLOPs = {FLOP_PER_CELL} per cell update (source code). Bytes = L1d load misses × 64 B.\n"
           "The grids fit in the 12 MB L3 (LLC misses ≈ 0), so L2 → L1 is the relevant roof; "
           "intensity w.r.t. DRAM is in the table.")


def fmt_ai(value):
    return "∞" if not math.isfinite(value) else f"{value:.2f}"


def make_table(summary, output_path: Path):
    lines = [
        "| Name | Ranks | N | T | Runs | Time (s) | Mcells/s | GFLOP/s | GFLOP/s/core "
        "| L1 miss (M/s) | LLC miss (M/s) | AI L2 (F/B) | AI DRAM (F/B) | FP counter / analytic |",
        "|------|------:|--:|--:|-----:|---------:|---------:|--------:|-------------:"
        "|--------------:|---------------:|------------:|--------------:|----------------------:|",
    ]
    order = {name: i for i, name in enumerate(SEQ_VARIANTS + [MPI_NAME])}
    for key in sorted(summary, key=lambda k: (order.get(k[0], 99), k[1], k[2], k[3])):
        name, ranks, n, t = key
        s = summary[key]
        lines.append(
            f"| {name} | {ranks} | {n} | {t} | {s['count']} "
            f"| {s['time']['mean']:.3f} ± {s['time']['std']:.3f} "
            f"| {s['cells_per_s']['mean'] / 1e6:.1f} | {s['gflops']['mean']:.2f} "
            f"| {s['gflops']['mean'] / max(ranks, 1):.2f} "
            f"| {s['l1_miss_per_s']['mean'] / 1e6:.1f} | {s['llc_miss_per_s']['mean'] / 1e6:.3f} "
            f"| {fmt_ai(s['ai_l2']['mean'])} | {fmt_ai(s['ai_dram']['mean'])} "
            f"| {s['fp_counter_ratio']['mean']:.2f} |"
        )
    note = ("\nFP counter / analytic: (fp_scalar + 2·fp_packed) / analytic FLOPs. FP_COMP_OPS_EXE counts "
            "executed SSE FP uops; a calibration loop with exactly 1e8 mul + 1e8 add read 3e8, "
            "so values above 1 are expected and the analytic count is used for GFLOP/s.\n")
    table = "### 2D heat stencil perf metrics\n\n" + "\n".join(lines) + "\n" + note
    output_path.write_text(table, encoding="utf-8")
    return table


def main():
    parser = argparse.ArgumentParser(description="Plot perf metrics (throughput, cache misses, roofline)")
    parser.add_argument("--csv", type=Path, default=Path(__file__).with_name("result.csv"),
                        help="Path to the benchmark CSV (with perf columns)")
    parser.add_argument("--output-dir", type=Path, default=Path(__file__).resolve().parent,
                        help="Directory for the generated plots and table")
    args = parser.parse_args()

    csv_path = args.csv.resolve()
    if not csv_path.exists():
        raise FileNotFoundError(f"Benchmark CSV not found: {csv_path}")

    runs, skipped = parse_csv(csv_path)
    if skipped:
        print(f"note: skipped {skipped} rows (not verified OK, or no perf counters)")
    if not runs:
        raise ValueError(f"No verified rows with perf counters in {csv_path}")
    summary = summarize(runs)

    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    outputs = {
        "throughput": output_dir / "heat_2D_perf_throughput.png",
        "cache": output_dir / "heat_2D_perf_cache_misses.png",
        "roofline": output_dir / "heat_2D_perf_roofline.png",
        "table": output_dir / "heat_2D_perf.md",
    }
    make_throughput_plot(summary, outputs["throughput"])
    make_cache_plot(summary, outputs["cache"])
    make_roofline_plot(summary, outputs["roofline"])
    table = make_table(summary, outputs["table"])

    for path in outputs.values():
        print(f"Saved {path}")
    print()
    print(table)


if __name__ == "__main__":
    main()
