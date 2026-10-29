CC=gcc
MPICC=mpicc
CFLAGS=-O2 -std=gnu11 -Wall -Wextra -pedantic

.PHONY: all
all: heat_stencil_1D_seq heat_stencil_1D_mpi_non_blocking heat_stencil_1D_mpi_blocking

heat_stencil_1D_seq: heat_stencil_1D_seq.c
	@$(CC) $(CFLAGS) $< -o $@

heat_stencil_1D_mpi_non_blocking: heat_stencil_1D_mpi_non_blocking.c
	@$(MPICC) $(CFLAGS) $< -o $@

heat_stencil_1D_mpi_blocking: heat_stencil_1D_mpi_blocking.c
	@$(MPICC) $(CFLAGS) $< -o $@

# -- benchmark builds: tuned for the CPU they are compiled on, so build them on the compute node --

BENCH_FLAGS=-Ofast -march=native -std=gnu11 -Wall -Wextra -pedantic
BUILD_DIR?=.

$(BUILD_DIR)/heat_stencil_1D_bench_blocking: heat_stencil_1D_mpi_blocking.c
	@$(MPICC) $(BENCH_FLAGS) $< -o $@

$(BUILD_DIR)/heat_stencil_1D_bench_non_blocking: heat_stencil_1D_mpi_non_blocking.c
	@$(MPICC) $(BENCH_FLAGS) $< -o $@

.PHONY: clean
clean:
	@rm -f heat_stencil_1D_seq heat_stencil_1D_mpi_non_blocking heat_stencil_1D_mpi_blocking
	@rm -f heat_stencil_1D_bench_blocking heat_stencil_1D_bench_non_blocking

.PHONY: run
run: all
	@echo "Sequential:"
	@./heat_stencil_1D_seq
	@echo
	@echo "MPI non-blocking (4 ranks):"
	@mpiexec -n 4 ./heat_stencil_1D_mpi_non_blocking
	@echo
	@echo "MPI blocking (4 ranks):"
	@mpiexec -n 4 ./heat_stencil_1D_mpi_blocking
	@echo
