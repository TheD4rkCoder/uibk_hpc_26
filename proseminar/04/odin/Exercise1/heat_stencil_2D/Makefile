CC=g++
CFLAGS=-Ofast -std=c++11 -Wall -Wextra -pedantic -g

.PHONY: all
all: heat_stencil_2D_seq

heat_stencil_2D_seq: heat_stencil_2D_seq.cpp
	@$(CC) $(CFLAGS) $< -o heat_stencil_2D_seq

.PHONY: clean
clean:
	@rm heat_stencil_2D_seq

.PHONY: run
run: all
	@echo "Sequential:"
	@./heat_stencil_2D_seq
	@echo
