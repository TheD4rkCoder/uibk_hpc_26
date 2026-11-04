# mpicc is plain gcc 12.2 here; the sequential version does not call MPI, but both
# are built with the same compiler so that the comparison is fair (system gcc is 8.5).
CC     = mpicc
CFLAGS = -O3 -march=native -std=gnu11 -Wall -Wextra -Werror

SEQ = heat_stencil_2D_seq heat_stencil_2D_seq_float
MPI = heat_stencil_2D heat_stencil_2D_blocking heat_stencil_2D_stripes heat_stencil_2D_nocomm \
      heat_stencil_2D_float heat_stencil_2D_float_blocking

all: $(SEQ) $(MPI)

heat_stencil_2D_seq: heat_stencil_2D_seq.c heat_stencil_2D.h
	$(CC) $(CFLAGS) $< -o $@

# -DFLOAT switches the whole program to single precision
heat_stencil_2D_seq_float: heat_stencil_2D_seq.c heat_stencil_2D.h
	$(CC) $(CFLAGS) -DFLOAT $< -o $@

# one source, the flags select the decomposition and the exchange variant

# non-blocking: start the exchange, compute the inner cells, wait, compute the frame
heat_stencil_2D: heat_stencil_2D.c heat_stencil_2D.h
	$(CC) $(CFLAGS) $< -o $@

# blocking: the exchange is finished before the compute starts
heat_stencil_2D_blocking: heat_stencil_2D.c heat_stencil_2D.h
	$(CC) $(CFLAGS) -DBLOCKING $< -o $@

# 1D decomposition, one horizontal stripe per rank
heat_stencil_2D_stripes: heat_stencil_2D.c heat_stencil_2D.h
	$(CC) $(CFLAGS) -DSTRIPES $< -o $@

# no ghost cell exchange at all: lower bound for the achievable time
heat_stencil_2D_nocomm: heat_stencil_2D.c heat_stencil_2D.h
	$(CC) $(CFLAGS) -DNOCOMM $< -o $@

heat_stencil_2D_float: heat_stencil_2D.c heat_stencil_2D.h
	$(CC) $(CFLAGS) -DFLOAT $< -o $@

heat_stencil_2D_float_blocking: heat_stencil_2D.c heat_stencil_2D.h
	$(CC) $(CFLAGS) -DFLOAT -DBLOCKING $< -o $@

clean:
	rm -f $(SEQ) $(MPI)

.PHONY: all clean
