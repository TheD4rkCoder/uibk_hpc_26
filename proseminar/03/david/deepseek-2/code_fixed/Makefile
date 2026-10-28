CC=mpicc
CFLAGS=-std=gnu11 -Wall -Wextra -pedantic -g -O2

.PHONY: all
all: example_1 example_2

example_1: example_1.c
	@$(CC) $(CFLAGS) $< -o example_1

example_2: example_2.c
	@$(CC) $(CFLAGS) $< -o example_2

.PHONY: clean
clean:
	@rm -f example_1 example_2
