SEQ_CC   = gcc
MPI_CC   = mpicc
CFLAGS   = -O3 -march=native -std=gnu11 -Wall -Wextra -pedantic

all: heat_stencil_1D_seq heat_stencil_1D_mpi

heat_stencil_1D_seq: heat_stencil_1D_seq.c
	$(SEQ_CC) $(CFLAGS) $< -o $@

heat_stencil_1D_mpi: heat_stencil_1D_mpi.c
	$(MPI_CC) $(CFLAGS) $< -o $@

clean:
	rm -f heat_stencil_1D_seq heat_stencil_1D_mpi

.PHONY: all clean