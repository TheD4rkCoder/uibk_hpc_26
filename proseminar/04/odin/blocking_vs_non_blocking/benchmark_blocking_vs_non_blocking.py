#!/usr/bin/env python3


import argparse
import csv
import subprocess
import sys
from pathlib import Path

# reuse the weak-scaling configuration of the 2D benchmark, so both benchmarks do the same runs
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "heat_stencil_2D"))
from benchmark_heat_2D import MPI_TIMESTEPS, WEAK_SCALING  # noqa: E402

VARIANTS = ["blocking", "non_blocking"]
# 1D weak scaling with the same work per rank as 2D: 768^2 / 96 = 6144 cells per rank
CELLS_PER_RANK_1D = 768 * 768 // 96


def problem_size(dim: str, ranks: int) -> int:
    return CELLS_PER_RANK_1D * ranks if dim == "1D" else WEAK_SCALING[ranks]


def run_job(script: Path, args: list, result_csv: Path, tasks: int) -> None:
    subprocess.run(
        ["sbatch", "--ntasks", str(tasks), str(script), *map(str, args), str(result_csv)],
        cwd=script.parent,
        check=True,
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Submit weak-scaling jobs comparing blocking and non-blocking ghost cell "
        "exchange for the 1D and 2D heat stencil."
    )
    parser.add_argument(
        "--dims",
        choices=["1D", "2D", "all"],
        default="all",
        help="Which stencil dimensions to benchmark.",
    )
    parser.add_argument(
        "--repetitions",
        type=int,
        default=1,
        help="How many times to repeat the full benchmark sweep.",
    )
    parser.add_argument(
        "--result-csv",
        type=Path,
        default=None,
        help="Path to the CSV file to generate.",
    )
    args = parser.parse_args()

    if args.repetitions < 1:
        parser.error("--repetitions must be at least 1")

    script_dir = Path(__file__).resolve().parent
    script = script_dir / "slurm_blocking_vs_non_blocking.sh"
    result_csv = (args.result_csv or script_dir / "result.csv").resolve()
    dims = ["1D", "2D"] if args.dims == "all" else [args.dims]

    result_csv.parent.mkdir(parents=True, exist_ok=True)
    with result_csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["dim", "variant", "ranks", "time", "verification", "N", "T"])

    for repetition in range(1, args.repetitions + 1):
        print(f"Starting repetition {repetition}/{args.repetitions}")
        for tasks in WEAK_SCALING:
            for dim in dims:
                size = problem_size(dim, tasks)
                for variant in VARIANTS:
                    print(f"Submitting {dim} {variant} for N={size}, T={MPI_TIMESTEPS}, ntasks={tasks}")
                    run_job(script, [dim, variant, size, MPI_TIMESTEPS], result_csv, tasks)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
