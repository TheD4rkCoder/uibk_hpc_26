#!/bin/bash

# Execute job in the partition "lva" unless you have special requirements.
#SBATCH --partition=lva

# Name your job to be able to identify it later
#SBATCH --job-name osu-micro-benchmark

# Redirect output stream to this file
#SBATCH --output=output.log

# scontrol show config -> SelectTypeParameters    = CR_CORE_MEMORY
# Maximum number of tasks (=processes) to start in total 😭
##SBATCH --ntasks=2

# scontrol show config -> SelectTypeParameters    = CR_CORE_MEMORY
# Maximum number of tasks (=processes) to start in total 🥸
#SBATCH --nodes=2

#scontrol show config -> SelectTypeParameters    = CR_CORE_MEMORY
# Maximum number of tasks (=processes) to start per node
#SBATCH --ntasks-per-node=2

# Enforce exclusive node allocation, do not share with other jobs
#SBATCH --exclusive

# LOAD MORE RECENT GCC!!!!
module load gcc/12.2.0-gcc-8.5.0-p4pe45v
module load openmpi/3.1.6-gcc-12.2.0-d2gmn55

# mpiexec is the default MPI launcher, is necessary, otherwise it is running on 1 core only.
# mpiexec without parameter starts as many processes as slots/processes created by sbatch
#  you can limit this by -n [X] for using X slots/processes
#echo "--- default ---" # default is binded by core
#mpiexec ./osu-micro-benchmarks-5.8/mpi/pt2pt/osu_bw
#mpiexec ./osu-micro-benchmarks-5.8/mpi/pt2pt/osu_latency

# Also note that the use of --use-hwthread-cpus changes the Open MPI’s definition 
# of a "processor element" from a processor core to a hardware thread. 
# See "DEFINITION OF ’PROCESSOR ELEMENT’", above. 
#echo "--- 0,1 ---"
#mpiexec --use-hwthread-cpus --cpu-list 0,1 --bind-to hwthread ./osu-micro-benchmarks-5.8/mpi/pt2pt/osu_bw
#mpiexec --use-hwthread-cpus --cpu-list 0,1 --bind-to hwthread ./osu-micro-benchmarks-5.8/mpi/pt2pt/osu_latency

#echo "--- 0,2 ---"
#mpiexec --use-hwthread-cpus --cpu-list 0,2 --bind-to hwthread ./osu-micro-benchmarks-5.8/mpi/pt2pt/osu_bw
#mpiexec --use-hwthread-cpus --cpu-list 0,2 --bind-to hwthread ./osu-micro-benchmarks-5.8/mpi/pt2pt/osu_latency

#echo "--- 0,12 ---"
#mpiexec --use-hwthread-cpus --cpu-list 0,12 --bind-to hwthread ./osu-micro-benchmarks-5.8/mpi/pt2pt/osu_bw
#mpiexec --use-hwthread-cpus --cpu-list 0,12 --bind-to hwthread ./osu-micro-benchmarks-5.8/mpi/pt2pt/osu_latency

#echo "--- different nodes ---"
#--bind-to core is actuallly already default
#mpiexec -map-by node --bind-to core ./osu-micro-benchmarks-5.8/mpi/pt2pt/osu_bw
#mpiexec -map-by node --bind-to core ./osu-micro-benchmarks-5.8/mpi/pt2pt/osu_latency

#!!!!!!!Another way to specify arbitrary mappings is with a rankfile,
# which gives you detailed control over process binding as well.
# https://www.open-mpi.org/doc/v3.1/man1/mpirun.1.php#toc10

OSU_BAW=./osu-micro-benchmarks-5.8/mpi/pt2pt/osu_bw
OSU_LAT=./osu-micro-benchmarks-5.8/mpi/pt2pt/osu_latency

export UCX_RNDV_THRESH=32768
#--report-bindings reports the bindings

echo "--- 1_ma_1_pa_1_co_2_th ---"
mpiexec --use-hwthread-cpus --rankfile rankfile_1_ma_1_pa_1_co_2_th.txt --report-bindings $OSU_BAW -m 1048576:1048576
mpiexec --use-hwthread-cpus --rankfile rankfile_1_ma_1_pa_1_co_2_th.txt --report-bindings $OSU_LAT -m 0:0

echo "--- 1_ma_1_pa_2_co_1_th ---"
mpiexec --use-hwthread-cpus --rankfile rankfile_1_ma_1_pa_2_co_1_th.txt --report-bindings $OSU_BAW -m 1048576:1048576
mpiexec --use-hwthread-cpus --rankfile rankfile_1_ma_1_pa_2_co_1_th.txt --report-bindings $OSU_LAT -m 0:0

echo "--- 1_ma_1_pa_2_co_2_th ---"
mpiexec --use-hwthread-cpus --rankfile rankfile_1_ma_1_pa_2_co_2_th.txt --report-bindings $OSU_BAW -m 1048576:1048576
mpiexec --use-hwthread-cpus --rankfile rankfile_1_ma_1_pa_2_co_2_th.txt --report-bindings $OSU_LAT -m 0:0

echo "--- 1_ma_2_pa_1_co_1_th ---"
mpiexec --use-hwthread-cpus --rankfile rankfile_1_ma_2_pa_1_co_1_th.txt --report-bindings $OSU_BAW -m 1048576:1048576
mpiexec --use-hwthread-cpus --rankfile rankfile_1_ma_2_pa_1_co_1_th.txt --report-bindings $OSU_LAT -m 0:0

echo "--- 1_ma_2_pa_1_co_2_th ---"
mpiexec --use-hwthread-cpus --rankfile rankfile_1_ma_2_pa_1_co_2_th.txt --report-bindings $OSU_BAW -m 1048576:1048576
mpiexec --use-hwthread-cpus --rankfile rankfile_1_ma_2_pa_1_co_2_th.txt --report-bindings $OSU_LAT -m 0:0

echo "--- 2_ma_1_pa_1_co_1_th ---"
mpiexec --use-hwthread-cpus --rankfile rankfile_2_ma_1_pa_1_co_1_th.txt --report-bindings $OSU_BAW -m 1048576:1048576
mpiexec --use-hwthread-cpus --rankfile rankfile_2_ma_1_pa_1_co_1_th.txt --report-bindings $OSU_LAT -m 0:0

echo "--- 2_ma_1_pa_1_co_2_th ---"
mpiexec --use-hwthread-cpus --rankfile rankfile_2_ma_1_pa_1_co_2_th.txt --report-bindings $OSU_BAW -m 1048576:1048576
mpiexec --use-hwthread-cpus --rankfile rankfile_2_ma_1_pa_1_co_2_th.txt --report-bindings $OSU_LAT -m 0:0

# WOW, BIG DIP!!!
# Reason:
'''
Unified Communication X (UCX) is a set of network APIs and their
implementations for high throughput computing.

The initial focus is on supporting semantics such as point-to-point communications
(one-sided and two-sided), collective communication, and remote atomic operations 
required for popular parallel programming models.

... support current network technologies such as:
- Open Fabrics - InfiniBand (Mellanox, Qlogic, IBM), libfabrics, iWARP, RoCE
- Cray GEMINI & ARIES
- Shared memory (MMAP, Posix, CMA, KNEM, XPMEM, etc.)
- Ethernet (TCP/UDP)

UCX picks a protocol for each message based on its size.
[cb761147@login.lcc3 2]$ ucx_info -c | grep THRES
UCX_BCOPY_THRESH=0 #bcopy
UCX_RNDV_THRESH=auto #rendezvous
UCX_RNDV_SEND_NBR_THRESH=256K #rendezvous
UCX_ZCOPY_THRESH=auto #zero copy

[cb761147@login.lcc3 2]$ ucx_info -c | grep MM_SEG
UCX_MM_SEG_SIZE=8256

UCX makes two separate choices for every message.
┌─────────────────────────────────────────┬────────────────────────────────┬─────────────────────────────────┐
│             Env Var                     │   Default (lcc3)               │            Protocol             │
├─────────────────────────────────────────┼────────────────────────────────┼─────────────────────────────────┤
│ < UCX_MM_FIFO_ELEM_SIZE                 │ 128 B                          │ eager, short                    │
├─────────────────────────────────────────┼────────────────────────────────┼─────────────────────────────────┤
│ ≤ UCX_MM_SEG_SIZE                       │ 8256 B                         │ eager, bcopy                    │
├─────────────────────────────────────────┼────────────────────────────────┼─────────────────────────────────┤
│ < UCX_RNDV_THRESH                       │ auto (≈ 8–16 KB)               │ eager, bcopy, split into pieces │
├─────────────────────────────────────────┼────────────────────────────────┼─────────────────────────────────┤
│ ≥ UCX_RNDV_THRESH                       │ auto (≈ 8–16 KB)               │ rendezvous, get_zcopy via CMA   │
└─────────────────────────────────────────┴────────────────────────────────┴─────────────────────────────────┘
'''

#Other reasons: Running out of buffers, cache effects, ...

# srun is the reccomended approach for production, part of slurm, not of MPI
# is srun has pmi2/pmix plugis, it is capable of running MPI applications
# srun 