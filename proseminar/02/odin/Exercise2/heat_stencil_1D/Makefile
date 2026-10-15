CC=gcc
MPICC=mpicc
CFLAGS=-O2 -std=gnu11 -Wall -Wextra -pedantic

.PHONY: all
all: heat_stencil_1D_seq heat_stencil_1D_mpi

heat_stencil_1D_seq: heat_stencil_1D_seq.c
	@$(CC) $(CFLAGS) $< -o heat_stencil_1D_seq

heat_stencil_1D_mpi: heat_stencil_1D_mpi.c
	@$(MPICC) $(CFLAGS) $< -o heat_stencil_1D_mpi

.PHONY: clean
clean:
	@rm -f heat_stencil_1D_seq heat_stencil_1D_mpi

.PHONY: run
run: all
	@echo "Sequential:"
	@./heat_stencil_1D_seq
	@echo
	@echo "MPI (4 ranks):"
	@mpiexec -n 4 ./heat_stencil_1D_mpi
	@echo
