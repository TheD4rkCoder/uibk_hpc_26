CXX := g++
MPICXX := mpic++
CXXFLAGS := -O3 -Wall -Wextra -pedantic

SEQ_SRC := pi_seq.cpp
MPI_SRC := pi_mpi.cpp

SEQ_BIN := pi_seq
MPI_BIN := pi_mpi

.PHONY: all clean

all: $(SEQ_BIN) $(MPI_BIN)

$(SEQ_BIN): $(SEQ_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(MPI_BIN): $(MPI_SRC)
	$(MPICXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(SEQ_BIN) $(MPI_BIN)