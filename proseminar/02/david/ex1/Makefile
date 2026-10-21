CXX      = mpic++
CXXFLAGS = -O3 -march=native -std=c++17

all: pi_seq pi_mpi

pi_seq: pi_seq.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

pi_mpi: pi_mpi.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f pi_seq pi_mpi

.PHONY: all clean