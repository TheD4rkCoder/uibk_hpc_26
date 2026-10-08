module load gcc/12.2.0-gcc-8.5.0-p4pe45v

cd osu-micro-benchmarks-5.8
./configure CC=mpicc CXX=mpic++

# this is necessary to avoid an unsupported version of automake needed for compilation:
touch configure Makefile.in Makefile

make

# this needs to be omitted, lack of permissions:
# make install