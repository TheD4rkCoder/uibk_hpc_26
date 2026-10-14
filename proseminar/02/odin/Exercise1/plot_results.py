#!/usr/bin/env python3


import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Tuple

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import ScalarFormatter


SAMPLE_SIZES = [10**power for power in range(3, 10)]
MPI_TASKS = [1, 2, 4, 8, 16, 32, 64, 96]


class ResultRow:
    def __init__(self, name, ranks, time, pi_estimate, samples):
        self.name = name
        self.ranks = ranks
        self.time = time
        self.pi_estimate = pi_estimate
        self.samples = samples


def load_rows(csv_path: Path) -> Tuple[List[ResultRow], List[int]]:
    with csv_path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        fieldnames = reader.fieldnames or []
        if not fieldnames:
            raise ValueError(f"{csv_path} does not contain a header row")

        rows = list(reader)
        if not rows:
            raise ValueError(f"{csv_path} does not contain any data rows")

        parsed_rows: List[ResultRow] = []
        sample_sizes_in_order: List[int] = []

        rows_per_sample = 1 + len(MPI_TASKS)
        rows_per_repetition = len(SAMPLE_SIZES) * rows_per_sample

        for index, row in enumerate(rows):
            name = row["name"].strip()
            ranks = int(row["ranks"])
            time = float(row["time"])
            pi_estimate = float(row["pi_estimate"])

            if "samples" in fieldnames:
                sample_size = int(row["samples"])
                sample_sizes_in_order.append(sample_size)
            else:
                if len(rows) % rows_per_repetition != 0:
                    raise ValueError(
                        f"Expected a multiple of {rows_per_repetition} data rows, found {len(rows)}"
                    )
                repetition_offset = index % rows_per_repetition
                sample_index = repetition_offset // rows_per_sample
                sample_size = SAMPLE_SIZES[sample_index]
                position_in_sample = repetition_offset % rows_per_sample

                if position_in_sample == 0:
                    expected_name = "pi_seq"
                    expected_ranks = 0
                else:
                    expected_name = "pi_mpi"
                    expected_ranks = MPI_TASKS[position_in_sample - 1]

                if name != expected_name:
                    raise ValueError(
                        f"Row {index + 2} has name {name!r}, expected {expected_name!r}"
                    )
                if ranks != expected_ranks:
                    raise ValueError(
                        f"Row {index + 2} has ranks {ranks}, expected {expected_ranks}"
                    )

            parsed_rows.append(
                ResultRow(
                    name=name,
                    ranks=ranks,
                    time=time,
                    pi_estimate=pi_estimate,
                    samples=sample_size,
                )
            )

    if "samples" in fieldnames:
        sample_sizes = sorted(set(sample_sizes_in_order))
    else:
        sample_sizes = SAMPLE_SIZES

    return parsed_rows, sample_sizes


def grouped_mean(rows: List[ResultRow], key_fields: Tuple[str, ...], value_field: str) -> Dict[Tuple, float]:
    buckets: Dict[Tuple, List[float]] = defaultdict(list)
    for row in rows:
        key = tuple(getattr(row, field) for field in key_fields)
        buckets[key].append(getattr(row, value_field))
    return {key: sum(values) / len(values) for key, values in buckets.items()}


def plot_mpi_time(rows: List[ResultRow], sample_sizes: List[int], output_path: Path) -> None:
    largest_sample = sample_sizes[-1]
    seq_mean = grouped_mean(rows, ("name", "samples"), "time")[("pi_seq", largest_sample)]
    mpi_means = grouped_mean(rows, ("name", "ranks", "samples"), "time")

    x_values = MPI_TASKS
    y_values = [mpi_means[("pi_mpi", tasks, largest_sample)] for tasks in MPI_TASKS]

    fig, ax = plt.subplots(figsize=(8, 5))
    ax.plot(x_values, y_values, marker="o", linewidth=2, label=f"MPI, n={largest_sample:,}")
    ax.axhline(seq_mean, linestyle="--", color="black", label=f"Sequential mean, n={largest_sample:,}")
    ax.set_xscale("log", basex=2)
    ax.set_xticks(MPI_TASKS)
    ax.set_yscale("log")
    ax.set_yscale("log", basey=2)
    ax.set_yticks([2**i for i in range(0,6)])
    ax.yaxis.set_major_formatter(ScalarFormatter())
    ax.xaxis.set_major_formatter(ScalarFormatter())
    ax.set_xlabel("Number of tasks")
    ax.set_ylabel("Time [s]")
    ax.set_title("MPI time vs tasks with sequential baseline")
    ax.grid(True, which="both", linestyle=":", linewidth=0.8)
    ax.legend()
    fig.tight_layout()
    fig.savefig(output_path, dpi=200)
    plt.close(fig)


def plot_seq_vs_mpi_time(rows: List[ResultRow], sample_sizes: List[int], output_path: Path) -> None:
    seq_means = grouped_mean(rows, ("name", "samples"), "time")
    mpi96_means = grouped_mean(rows, ("name", "ranks", "samples"), "time")

    x_values = sample_sizes
    seq_values = [seq_means[("pi_seq", sample)] for sample in sample_sizes]
    mpi_values = [mpi96_means[("pi_mpi", 96, sample)] for sample in sample_sizes]

    fig, ax = plt.subplots(figsize=(8, 5))
    ax.plot(x_values, seq_values, marker="o", linewidth=2, label="Sequential")
    ax.plot(x_values, mpi_values, marker="o", linewidth=2, label="MPI, 96 tasks")
    ax.set_xscale("log")
    ax.set_xlabel("Number of samples")
    ax.set_ylabel("Time [s]")
    ax.set_title("Sequential vs MPI time")
    ax.grid(True, which="both", linestyle=":", linewidth=0.8)
    ax.legend()
    fig.tight_layout()
    fig.savefig(output_path, dpi=200)
    plt.close(fig)


def plot_seq_vs_mpi_error(rows: List[ResultRow], sample_sizes: List[int], output_path: Path) -> None:
    seq_means = grouped_mean(rows, ("name", "samples"), "pi_estimate")
    mpi96_means = grouped_mean(rows, ("name", "ranks", "samples"), "pi_estimate")

    x_values = sample_sizes
    seq_errors = [abs(seq_means[("pi_seq", sample)] - math.pi) for sample in sample_sizes]
    mpi_errors = [abs(mpi96_means[("pi_mpi", 96, sample)] - math.pi) for sample in sample_sizes]

    fig, ax = plt.subplots(figsize=(8, 5))
    ax.plot(x_values, seq_errors, marker="o", linewidth=2, label="Sequential")
    ax.plot(x_values, mpi_errors, marker="o", linewidth=2, label="MPI, 96 tasks")
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Number of samples")
    ax.set_ylabel("Absolute error to π")
    ax.set_title("Sequential vs MPI error to π")
    ax.grid(True, which="both", linestyle=":", linewidth=0.8)
    ax.legend()
    fig.tight_layout()
    fig.savefig(output_path, dpi=200)
    plt.close(fig)


def write_time_table(rows: List[ResultRow], sample_sizes: List[int], output_path: Path) -> None:
    largest_sample = sample_sizes[-1]
    seq_mean = grouped_mean(rows, ("name", "samples"), "time")[("pi_seq", largest_sample)]
    mpi_means = grouped_mean(rows, ("name", "ranks", "samples"), "time")

    with output_path.open("w", encoding="utf-8") as handle:
        handle.write(f"# Time table for n = {largest_sample:,}\n\n")
        handle.write("| ranks | sequential time [s] | mpi time [s] | speedup vs seq |\n")
        handle.write("| --- | ---: | ---: | ---: |\n")
        handle.write(
            "| seq | {seq_time:.6f} | - | 1.000 |\n".format(seq_time=seq_mean)
        )

        for ranks in MPI_TASKS:
            mpi_time = mpi_means[("pi_mpi", ranks, largest_sample)]
            speedup = seq_mean / mpi_time if mpi_time else float("inf")
            handle.write(
                "| {ranks} | {seq_time:.6f} | {mpi_time:.6f} | {speedup:.3f} |\n".format(
                    ranks=ranks,
                    seq_time=seq_mean,
                    mpi_time=mpi_time,
                    speedup=speedup,
                )
            )


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate plots from results.csv.")
    parser.add_argument(
        "--results-csv",
        type=Path,
        default=Path(__file__).resolve().parent / "result.csv",
        help="Path to the benchmark CSV file.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).resolve().parent / "plots",
        help="Directory where the plots will be written.",
    )
    args = parser.parse_args()

    rows, sample_sizes = load_rows(args.results_csv.resolve())
    args.output_dir.mkdir(parents=True, exist_ok=True)

    plot_mpi_time(rows, sample_sizes, args.output_dir / "mpi_time_vs_tasks.png")
    plot_seq_vs_mpi_time(rows, sample_sizes, args.output_dir / "seq_vs_mpi96_time.png")
    plot_seq_vs_mpi_error(rows, sample_sizes, args.output_dir / "seq_vs_mpi96_error.png")
    write_time_table(rows, sample_sizes, args.output_dir / "time_table.md")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())