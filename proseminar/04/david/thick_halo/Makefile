CC = mpicc
CFLAGS = -O3 -march=native -std=gnu11 -Wall -Wextra
HALOS = 1 2 4 8
TARGETS = $(addprefix thick_stripe,$(HALOS))

all: $(TARGETS)

$(TARGETS): thick_stripe%: thick_stripe.c
	$(CC) $(CFLAGS) -DHALO=$* -o $@ $<

clean:
	rm -f $(TARGETS)
