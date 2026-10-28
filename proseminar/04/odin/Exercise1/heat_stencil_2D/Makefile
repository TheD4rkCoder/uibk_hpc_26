CC=g++
MPICC=mpic++
CFLAGS=-Ofast -std=c++11 -Wall -Wextra -pedantic -g

# benchmark builds: tuned for the CPU they are compiled on, so build them on the compute node
BENCH_FLAGS=-Ofast -march=native -std=c++11 -Wall -Wextra -pedantic
BUILD_DIR?=.
BENCH_VARIANTS=naive tiling ghost current

.PHONY: all
all: heat_stencil_2D_seq

heat_stencil_2D_seq: heat_stencil_2D_seq.cpp
	@$(CC) $(CFLAGS) $< -o heat_stencil_2D_seq

# -- benchmark variants --

.PHONY: bench
bench: $(addprefix $(BUILD_DIR)/heat_stencil_2D_,$(BENCH_VARIANTS))

# flat array, boundary/source checks in the inner loop, no tiling
$(BUILD_DIR)/heat_stencil_2D_naive: heat_stencil_2D_seq_naive.cpp
	@$(CC) $(BENCH_FLAGS) $< -o $@

# naive + spatial tiling
$(BUILD_DIR)/heat_stencil_2D_tiling: heat_stencil_2D_seq_tiled.cpp
	@$(CC) $(BENCH_FLAGS) $< -o $@

# ghost cells without tiling: one tile spans the whole grid
$(BUILD_DIR)/heat_stencil_2D_ghost: heat_stencil_2D_seq.cpp
	@$(CC) $(BENCH_FLAGS) -DTILE=1000000 $< -o $@

# ghost cells + tiling
$(BUILD_DIR)/heat_stencil_2D_current: heat_stencil_2D_seq.cpp
	@$(CC) $(BENCH_FLAGS) $< -o $@

# MPI version (2D block decomposition)
$(BUILD_DIR)/heat_stencil_2D_mpi: heat_stencil_2D_mpi.cpp
	@$(MPICC) $(BENCH_FLAGS) $< -o $@

.PHONY: clean
clean:
	@rm -f heat_stencil_2D_seq heat_stencil_2D_mpi $(addprefix heat_stencil_2D_,$(BENCH_VARIANTS))

.PHONY: run
run: all
	@echo "Sequential:"
	@./heat_stencil_2D_seq
	@echo
