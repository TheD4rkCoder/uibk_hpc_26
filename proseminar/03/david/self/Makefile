CC=mpicc
CFLAGS=-std=gnu11 -g -Wall -Wextra -pedantic -O1 # -fsanitize=address,undefined 

.PHONY: all
all: example_1 example_2

example_1: example_1_copy.c
	@$(CC) $(CFLAGS) $< -o example_1

example_2: example_2_copy.c
	@$(CC) $(CFLAGS) $< -o example_2

.PHONY: clean
clean:
	@rm example_1 example_2
