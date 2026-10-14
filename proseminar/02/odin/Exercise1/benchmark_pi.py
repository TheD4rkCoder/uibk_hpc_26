#!/usr/bin/env python3


import argparse
import csv
import subprocess
import sys
from pathlib import Path


SAMPLE_SIZES = [10**power for power in range(3, 10)]
MPI_TASKS = [1, 2, 4, 8, 16, 32, 64, 96]


def run_job(script: Path, samples: int, result_csv: Path, tasks: int = None) -> None:
    command = ["sbatch"]
    if tasks is not None:
        command.extend(["--ntasks", str(tasks)])
    command.extend([str(script), str(samples), str(result_csv)])

    subprocess.run(
        command,
        cwd=script.parent,
        check=True,
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Submit pi benchmark jobs for sequential and MPI implementations."
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
    seq_script = (args.seq_script or script_dir / "slurm_pi_seq.sh").resolve()
    mpi_script = (args.mpi_script or script_dir / "slurm_pi_mpi.sh").resolve()
    result_csv = (args.result_csv or script_dir / "result.csv").resolve()

    if not seq_script.exists():
        parser.error(f"Sequential SLURM script not found: {seq_script}")
    if not mpi_script.exists():
        parser.error(f"MPI SLURM script not found: {mpi_script}")

    result_csv.parent.mkdir(parents=True, exist_ok=True)
    with result_csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["name", "ranks", "time", "pi_estimate", "samples"])

    for repetition in range(1, args.repetitions + 1):
        print(f"Starting repetition {repetition}/{args.repetitions}")
        for samples in SAMPLE_SIZES:
            print(f"Submitting pi_seq for n={samples}")
            run_job(seq_script, samples, result_csv)
            for tasks in MPI_TASKS:
                print(f"Submitting pi_mpi for n={samples}, ntasks={tasks}")
                run_job(mpi_script, samples, result_csv, tasks=tasks)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())