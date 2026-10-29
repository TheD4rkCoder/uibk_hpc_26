#!/usr/bin/env python3

import argparse
import csv
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import NullLocator
import numpy as np

# same chart style as the 2D weak-scaling plots
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "heat_stencil_2D"))
from visualize_weak_scaling import (  # noqa: E402
    COLOR_MPI, COLOR_SEQ, MUTED, SURFACE, TEXT_PRIMARY, TEXT_SECONDARY,
    label_line_end, style_axes, summarize_runs)

DIMS = ["1D", "2D"]
COLOR_BLOCKING = COLOR_MPI         # categorical slot 1
COLOR_NON_BLOCKING = COLOR_SEQ     # categorical slot 2
COLOR_DIFF = "#6250d6"             # differences are their own quantity, not one of the two variants


def parse_csv(csv_path: Path):
    """Group run times by (dim, variant, ranks, N, T); runs that did not verify OK are skipped."""
    data = defaultdict(list)
    skipped = []
    with csv_path.open("r", newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            # check verification first: crashed runs may have an empty time field
            if (row.get("verification") or "").strip() != "OK":
                skipped.append(row)
                continue
            try:
                key = (row["dim"].strip(), row["variant"].strip(), int(row["ranks"]),
                       int(row["N"]), int(row["T"]))
                time = float(row["time"])
            except (KeyError, TypeError, ValueError):
                continue
            data[key].append(time)
    return data, skipped


def collect(data, dim):
    """Points where both variants were measured with the same ranks, N and T."""
    points = []
    for (d, variant, ranks, n, t), times in data.items():
        if d != dim or variant != "blocking":
            continue
        other = data.get((dim, "non_blocking", ranks, n, t))
        if not other:
            print(f"warning: no non_blocking run for {dim}, {ranks} ranks, N={n} -> skipped")
            continue
        blk, nb = summarize_runs(times), summarize_runs(other)

        # positive differences = non-blocking is faster
        abs_diff = blk["mean"] - nb["mean"]
        abs_err = float(np.hypot(blk["std"], nb["std"]))
        ratio = nb["mean"] / blk["mean"]
        rel_err = ratio * float(np.hypot(blk["std"] / blk["mean"], nb["std"] / nb["mean"]))
        points.append({
            "ranks": ranks, "N": n, "T": t, "blocking": blk, "non_blocking": nb,
            "abs_diff": abs_diff, "abs_err": abs_err,
            "rel_diff": (1 - ratio) * 100, "rel_err": rel_err * 100,
        })
    points.sort(key=lambda p: p["ranks"])
    return points


def compact(n):
    # short tick labels for the large 1D sizes: 6144 -> 6.1k, 589824 -> 590k
    if n < 1000:
        return str(n)
    return f"{n / 1000:.1f}k" if n < 10000 else f"{n / 1000:.0f}k"


def rank_axis(ax, points):
    # x: log2 rank axis, every measured rank count labeled together with its N
    ranks = [p["ranks"] for p in points]
    ax.set_xscale("log", base=2)
    ax.set_xticks(ranks)
    ax.set_xticklabels([f"{p['ranks']}\n{compact(p['N'])}" for p in points], fontsize=7)
    ax.xaxis.set_minor_locator(NullLocator())
    ax.set_xlim(ranks[0] / 1.2, ranks[-1] * 1.9)  # room for the end labels
    ax.set_xlabel("Ranks p  (problem size N below)", color=TEXT_SECONDARY)


def footnote(points_by_dim):
    reps = sorted({p[v]["count"] for pts in points_by_dim.values() for p in pts
                   for v in ("blocking", "non_blocking")})
    rep_text = str(reps[0]) if len(reps) == 1 else f"{reps[0]}–{reps[-1]}"
    t_steps = next(p["T"] for pts in points_by_dim.values() for p in pts)
    return (f"Weak scaling, ~6144 cells per rank, T={t_steps}. Points: mean of {rep_text} repetitions; "
            "error bars: ±1 sample std (differences: propagated from both variants).")


def new_figure(points_by_dim, title):
    dims = [d for d in DIMS if points_by_dim.get(d)]
    fig, axes = plt.subplots(1, len(dims), figsize=(7.5 * len(dims), 5), sharey=True, squeeze=False)
    fig.patch.set_facecolor(SURFACE)
    fig.suptitle(title, color=TEXT_PRIMARY, x=0.01, ha="left", fontsize=12)
    return fig, dict(zip(dims, axes[0]))


def finish(fig, points_by_dim, output_path: Path):
    fig.text(0.01, 0.005, footnote(points_by_dim), fontsize=8, color=TEXT_SECONDARY)
    fig.tight_layout(rect=(0, 0.03, 1, 1))
    fig.savefig(output_path, dpi=200, facecolor=SURFACE)
    plt.close(fig)


def label_end_pair(ax, x, y_blocking, y_non_blocking, min_gap_px=12):
    # direct labels at the line ends; if they would overlap, push them apart symmetrically
    to_px = lambda y: ax.transData.transform((x, y))[1]
    gap = abs(to_px(y_blocking) - to_px(y_non_blocking))
    shift = max(0.0, (min_gap_px - gap) / 2)
    upper_is_blocking = y_blocking >= y_non_blocking
    for y, text, is_upper in [(y_blocking, "blocking", upper_is_blocking),
                              (y_non_blocking, "non-blocking", not upper_is_blocking)]:
        ax.annotate(text, (x, y), xytext=(10, shift if is_upper else -shift), textcoords="offset points",
                    va="center", fontsize=9, color=TEXT_PRIMARY)


errorbar_style = dict(linewidth=2, markersize=6, capsize=3, elinewidth=1.2)


def make_runtime_plot(points_by_dim, output_path: Path):
    fig, axes = new_figure(points_by_dim, "Heat stencil: blocking vs. non-blocking ghost cell exchange")
    for dim, ax in axes.items():
        points = points_by_dim[dim]
        ranks = np.array([p["ranks"] for p in points], dtype=float)
        for variant, color, marker, label in [("blocking", COLOR_BLOCKING, "o", "blocking"),
                                              ("non_blocking", COLOR_NON_BLOCKING, "s", "non-blocking")]:
            means = np.array([p[variant]["mean"] for p in points])
            stds = np.array([p[variant]["std"] for p in points])
            ax.errorbar(ranks, means, yerr=stds, fmt=marker + "-", color=color, label=label, **errorbar_style)
        ax.set_title(f"{dim} stencil", color=TEXT_PRIMARY, loc="left", fontsize=11)
        rank_axis(ax, points)
        style_axes(ax)
    # shared y axis from 0 up to the slowest run of both panels
    top = max(p[v]["mean"] + p[v]["std"] for pts in points_by_dim.values() for p in pts
              for v in ("blocking", "non_blocking"))
    first = next(iter(axes.values()))
    first.set_ylim(0, top * 1.1)
    first.set_ylabel("Wall time (s)", color=TEXT_SECONDARY)
    fig.tight_layout(rect=(0, 0.03, 1, 1))
    for dim, ax in axes.items():
        last = points_by_dim[dim][-1]
        label_end_pair(ax, last["ranks"], last["blocking"]["mean"], last["non_blocking"]["mean"])
    first.legend(frameon=False, fontsize=9, labelcolor=TEXT_PRIMARY, loc="lower right")
    finish(fig, points_by_dim, output_path)


def make_difference_plot(points_by_dim, output_path: Path, metric: str, ylabel: str, title: str, unit: str):
    fig, axes = new_figure(points_by_dim, title)
    for dim, ax in axes.items():
        points = points_by_dim[dim]
        ranks = np.array([p["ranks"] for p in points], dtype=float)
        values = np.array([p[metric] for p in points])
        errors = np.array([p[metric.replace("diff", "err")] for p in points])

        ax.axhline(0, color=MUTED, linestyle="--", linewidth=1)
        ax.errorbar(ranks, values, yerr=errors, fmt="o-", color=COLOR_DIFF, **errorbar_style)
        label_line_end(ax, ranks[-1], values[-1], f"{values[-1]:+.{3 if unit == 's' else 1}f} {unit}")
        ax.set_title(f"{dim} stencil", color=TEXT_PRIMARY, loc="left", fontsize=11)
        rank_axis(ax, points)
        style_axes(ax)

    first = next(iter(axes.values()))
    first.set_ylabel(ylabel, color=TEXT_SECONDARY)
    # say which direction is better, next to the zero line
    first.annotate("above 0: non-blocking faster", (0.02, 0.97), xycoords="axes fraction",
                   va="top", fontsize=8, color=TEXT_SECONDARY)
    finish(fig, points_by_dim, output_path)


def make_table(points_by_dim, output_path: Path):
    lines = [
        "| Dim | Ranks | N | Blocking (s) | Non-blocking (s) | Runs (b/nb) | Abs. diff (s) | Rel. diff (%) |",
        "|-----|------:|--:|-------------:|-----------------:|------------:|--------------:|--------------:|",
    ]
    for dim, points in points_by_dim.items():
        for p in points:
            b, nb = p["blocking"], p["non_blocking"]
            lines.append(
                f"| {dim} | {p['ranks']} | {p['N']} | {b['mean']:.3f} ± {b['std']:.3f} "
                f"| {nb['mean']:.3f} ± {nb['std']:.3f} | {b['count']}/{nb['count']} "
                f"| {p['abs_diff']:+.3f} ± {p['abs_err']:.3f} | {p['rel_diff']:+.1f} ± {p['rel_err']:.1f} |"
            )
    table = ("### Blocking vs. non-blocking ghost cell exchange\n\n"
             "Differences: blocking − non-blocking; positive = non-blocking faster. "
             "Relative difference w.r.t. the blocking time.\n\n" + "\n".join(lines) + "\n")
    output_path.write_text(table, encoding="utf-8")
    return table


def main():
    parser = argparse.ArgumentParser(description="Plot the blocking vs. non-blocking benchmark")
    parser.add_argument("--csv", type=Path, default=Path(__file__).with_name("result.csv"),
                        help="Path to the benchmark CSV")
    parser.add_argument("--output-dir", type=Path, default=Path(__file__).resolve().parent,
                        help="Directory for the generated plots and table")
    args = parser.parse_args()

    csv_path = args.csv.resolve()
    if not csv_path.exists():
        raise FileNotFoundError(f"Benchmark CSV not found: {csv_path}")

    data, skipped = parse_csv(csv_path)
    for row in skipped:
        print(f"warning: skipped run that did not verify: {dict(row)}")

    points_by_dim = {dim: collect(data, dim) for dim in DIMS}
    points_by_dim = {dim: points for dim, points in points_by_dim.items() if points}
    if not points_by_dim:
        raise ValueError(f"No matching blocking/non_blocking pairs found in {csv_path}")

    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    outputs = {
        "runtime": output_dir / "blocking_vs_non_blocking_runtime.png",
        "abs": output_dir / "blocking_vs_non_blocking_abs_diff.png",
        "rel": output_dir / "blocking_vs_non_blocking_rel_diff.png",
        "table": output_dir / "blocking_vs_non_blocking.md",
    }
    make_runtime_plot(points_by_dim, outputs["runtime"])
    make_difference_plot(points_by_dim, outputs["abs"], "abs_diff", "T_blocking − T_non-blocking (s)",
                         "Absolute difference: blocking − non-blocking", "s")
    make_difference_plot(points_by_dim, outputs["rel"], "rel_diff", "(T_blocking − T_non-blocking) / T_blocking (%)",
                         "Relative difference: time saved by the non-blocking exchange", "%")
    table = make_table(points_by_dim, outputs["table"])

    for path in outputs.values():
        print(f"Saved {path}")
    print()
    print(table)


if __name__ == "__main__":
    main()
