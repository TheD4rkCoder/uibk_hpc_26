# Requires an MPI compiler.  The workspace-local MPICH is used if present:
#   source .mpi/env.sh
export MPICH_CC = gcc
MPICC := $(if $(wildcard .mpi/env/bin/mpicc),.mpi/env/bin/mpicc,mpicc)
CC = $(MPICC)
CFLAGS ?= -std=gnu11 -Wall -Wextra

.PHONY: all broken clean test repro

all: example_1_fixed example_2_fixed

# The fixed programs.
example_1_fixed: example_1_fixed.c
	$(CC) $(CFLAGS) $< -o $@

example_2_fixed: example_2_fixed.c
	$(CC) $(CFLAGS) $< -o $@

# The original, buggy programs (kept for reproducing the hangs).
broken: example_1 example_2

example_1: example_1.c
	$(CC) $(CFLAGS) $< -o $@

example_2: example_2.c
	$(CC) $(CFLAGS) $< -o $@

test: all
	./verify.sh

# Minimal failing tests for each individual bug.
repro:
	./tests/run.sh

clean:
	rm -f example_1 example_2 example_1_fixed example_2_fixed
	rm -rf build
