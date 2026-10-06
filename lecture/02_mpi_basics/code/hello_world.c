#include <mpi.h>
#include <stdio.h>

int main(int argc, char** argv) {
	MPI_Init(&argc, &argv); // initialize the MPI environment

	int size;
	MPI_Comm_size(MPI_COMM_WORLD, &size); // get the number of ranks

	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank); // get the rank of the caller

	printf("Hello world from rank %d of %d\n", rank, size);

	MPI_Finalize(); // cleanup
}
