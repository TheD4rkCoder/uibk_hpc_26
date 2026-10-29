#!/usr/bin/env python3


import argparse
import csv
import subprocess
from pathlib import Path


PROBLEM_SIZES = [256, 512, 768]
# naive -> + tiling -> ghost cells (no tiling) -> ghost cells + tiling
VARIANTS = ["naive", "tiling", "ghost", "current"]

# weak scaling: constant work per rank, anchored at 96 ranks with N=768 -> N^2/p = 6144 cells.
# N ~ sqrt(6144 * p), rounded to a multiple of the MPI_Dims_create grid so every rank gets
# an equal block (within ~3% of 6144 cells/rank). Ranks: 2^n and 2^n + 2^(n-1) up to 96.
WEAK_SCALING = {
    1: 78,     # 1x1
    2: 110,    # 2x1
    3: 135,    # 3x1
    4: 156,    # 2x2
    6: 192,    # 3x2
    8: 220,    # 4x2
    12: 276,   # 4x3
    16: 312,   # 4x4
    24: 384,   # 6x4
    32: 440,   # 8x4
    48: 552,   # 8x6
    64: 624,   # 8x8
    96: 768,   # 12x8
}
# fixed number of timesteps, so the work per rank stays constant (T = N*100 would grow with N)
MPI_TIMESTEPS = 768 * 100
# fastest sequential variant, run at every weak-scaling N as the baseline for speedup/efficiency
BASELINE_VARIANT = "ghost"


def run_job(script: Path, args: list, result_csv: Path, tasks: int = None) -> None:
    command = ["sbatch"]
    if tasks is not None:
        command.extend(["--ntasks", str(tasks)])
    command.extend([str(script), *map(str, args), str(result_csv)])

    subprocess.run(
        command,
        cwd=script.parent,
        check=True,
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Submit 2D heat stencil benchmark jobs: sequential optimization variants "
        "and MPI weak scaling."
    )
    parser.add_argument(
        "--suite",
        choices=["seq", "mpi", "all"],
        default="all",
        help="Which benchmarks to submit.",
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
    seq_script = (args.seq_script or script_dir / "slurm_heat_2D_seq.sh").resolve()
    mpi_script = (args.mpi_script or script_dir / "slurm_heat_2D_mpi.sh").resolve()
    result_csv = (args.result_csv or script_dir / "result.csv").resolve()

    run_seq = args.suite in ("seq", "all")
    run_mpi = args.suite in ("mpi", "all")

    if run_seq and not seq_script.exists():
        parser.error(f"Sequential SLURM script not found: {seq_script}")
    if run_mpi and not mpi_script.exists():
        parser.error(f"MPI SLURM script not found: {mpi_script}")

    result_csv.parent.mkdir(parents=True, exist_ok=True)
    with result_csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(["name", "ranks", "time", "verification", "N", "T"])

    for repetition in range(1, args.repetitions + 1):
        print(f"Starting repetition {repetition}/{args.repetitions}")
        if run_seq:
            for size in PROBLEM_SIZES:
                for variant in VARIANTS:
                    print(f"Submitting heat_stencil_2D {variant} for N={size}, T={size * 100}")
                    run_job(seq_script, [variant, size, size * 100], result_csv)
        if run_mpi:
            for tasks, size in WEAK_SCALING.items():
                print(f"Submitting heat_stencil_2D {BASELINE_VARIANT} baseline for N={size}, T={MPI_TIMESTEPS}")
                run_job(seq_script, [BASELINE_VARIANT, size, MPI_TIMESTEPS], result_csv)
                print(f"Submitting heat_stencil_2D_mpi for N={size}, T={MPI_TIMESTEPS}, ntasks={tasks}")
                run_job(mpi_script, [size, MPI_TIMESTEPS], result_csv, tasks=tasks)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
