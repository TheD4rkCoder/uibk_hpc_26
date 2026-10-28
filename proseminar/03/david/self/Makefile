CC=mpicc
CFLAGS=-std=gnu11 -g -Wall -Wextra -pedantic -fsanitize=address,undefined -O1

.PHONY: all
all: example_1 example_2

example_1: example_1.c
	@$(CC) $(CFLAGS) $< -o example_1

example_2: example_2.c
	@$(CC) $(CFLAGS) $< -o example_2

.PHONY: clean
clean:
	@rm example_1 example_2
