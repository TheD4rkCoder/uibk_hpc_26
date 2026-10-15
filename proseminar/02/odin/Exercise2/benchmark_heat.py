#!/usr/bin/env python3


import argparse
import csv
import subprocess
import sys
from pathlib import Path


# 2^9, 2^9+2^8, 2^10, 2^10+2^9, ..., 2^12+2^11
PROBLEM_SIZES = [size for n in range(9, 13) for size in (2**n, 2**n + 2**(n - 1))]
MPI_TASKS = [1, 2, 4, 8, 16, 32, 64, 96]


def run_job(script: Path, size: int, result_csv: Path, tasks: int = None) -> None:
    command = ["sbatch"]
    if tasks is not None:
        command.extend(["--ntasks", str(tasks)])
    command.extend([str(script), str(size), str(result_csv)])

    subprocess.run(
        command,
        cwd=script.parent,
        check=True,
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Submit heat stencil benchmark jobs for sequential and MPI implementations."
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
    parser.add_argument(
        "--seq-script",
        type=Path,
        default=None,
        help="Path to the sequential SLURM script.",
    )
    parser.add_argument(
        "--mpi-script",
        type=Path,
        default=None,
        help="Path to the MPI SLURM script.",
    )
    args = parser.parse_args()

    if args.repetitions < 1:
        parser.error("--repetitions must be at least 1")

    script_dir = Path(__file__).resolve().parent
    seq_script = (args.seq_script or script_dir / "slurm_heat_seq.sh").resolve()
    mpi_script = (args.mpi_script or script_dir / "slurm_heat_mpi.sh").resolve()
    result_csv = (args.result_csv or script_dir / "result.csv").resolve()

    if not seq_script.exists():
        parser.error(f"Sequential SLURM script not found: {seq_script}")
    if not mpi_script.exists():
        parser.error(f"MPI SLURM script not found: {mpi_script}")

    result_csv.parent.mkdir(parents=True, exist_ok=True)
    with result_csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["name", "ranks", "time", "verification", "N"])

    for repetition in range(1, args.repetitions + 1):
        print(f"Starting repetition {repetition}/{args.repetitions}")
        for size in PROBLEM_SIZES:
            print(f"Submitting heat_stencil_1D_seq for N={size}")
            run_job(seq_script, size, result_csv)
            for tasks in MPI_TASKS:
                print(f"Submitting heat_stencil_1D_mpi for N={size}, ntasks={tasks}")
                run_job(mpi_script, size, result_csv, tasks=tasks)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())