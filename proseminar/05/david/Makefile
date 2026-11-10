CXX      ?= g++
# The lab machines have g++ 8.5 in the default path; module load
# gcc/12.2.0-gcc-8.5.0-p4pe45v gives g++ 12.2, which is what the benchmarks use.
CXXFLAGS ?= -O3 -march=native -std=c++17 -Wall -Wextra -Werror -pedantic
CPPFLAGS ?= -Isrc

COMMON_SRC = src/nbody.cpp src/octree.cpp
COMMON_HDR = src/nbody.h src/octree.h

BINS = nbody nbody_quad

all: $(BINS) validate validate_quad

nbody: src/main.cpp $(COMMON_SRC) $(COMMON_HDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/main.cpp $(COMMON_SRC) -o $@

nbody_quad: src/main.cpp $(COMMON_SRC) $(COMMON_HDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DQUADRUPOLE src/main.cpp $(COMMON_SRC) -o $@

validate: tests/validate.cpp $(COMMON_SRC) $(COMMON_HDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/validate.cpp $(COMMON_SRC) -o $@

validate_quad: tests/validate.cpp $(COMMON_SRC) $(COMMON_HDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DQUADRUPOLE tests/validate.cpp $(COMMON_SRC) -o $@

# gprof build; only used for the profiling section of results.md
nbody_gprof: src/main.cpp $(COMMON_SRC) $(COMMON_HDR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -g -pg -fno-inline src/main.cpp $(COMMON_SRC) -o $@

clean:
	rm -f $(BINS) validate validate_quad nbody_gprof gmon.out

.PHONY: all clean
