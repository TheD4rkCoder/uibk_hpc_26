#!/usr/bin/env python3


import argparse
import csv
import subprocess
from pathlib import Path


PROBLEM_SIZES = [256, 512]
# naive -> + tiling -> ghost cells (no tiling) -> ghost cells + tiling
VARIANTS = ["naive", "tiling", "ghost", "current"]


def run_job(script: Path, variant: str, size: int, result_csv: Path) -> None:
    subprocess.run(
        ["sbatch", str(script), variant, str(size), str(result_csv)],
        cwd=script.parent,
        check=True,
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Submit 2D heat stencil benchmark jobs for the sequential optimization variants."
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
    args = parser.parse_args()

    if args.repetitions < 1:
        parser.error("--repetitions must be at least 1")

    script_dir = Path(__file__).resolve().parent
    seq_script = (args.seq_script or script_dir / "slurm_heat_2D_seq.sh").resolve()
    result_csv = (args.result_csv or script_dir / "result.csv").resolve()

    if not seq_script.exists():
        parser.error(f"Sequential SLURM script not found: {seq_script}")

    result_csv.parent.mkdir(parents=True, exist_ok=True)
    with result_csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["name", "ranks", "time", "verification", "N"])

    for repetition in range(1, args.repetitions + 1):
        print(f"Starting repetition {repetition}/{args.repetitions}")
        for size in PROBLEM_SIZES:
            for variant in VARIANTS:
                print(f"Submitting heat_stencil_2D {variant} for N={size}")
                run_job(seq_script, variant, size, result_csv)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
