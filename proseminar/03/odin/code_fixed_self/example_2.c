#include <mpi.h>
#include <stdio.h>

int main(int argc, char** argv) {
	int rank;
	int size;
	int sBuf[2] = { 1, 2 };
	int rBuf[2];
	MPI_Status status;
	MPI_Datatype newType;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	// Enough tasks?
	if(size < 2) {
		printf("This test needs at least 2 processes ! \n");
		MPI_Finalize();
		return 1;
	}

	// Say hello
	printf("Hello, I am rank %d of %d processes. \n", rank, size);

	//) Create a datatype
	MPI_Type_contiguous(2, MPI_INT, &newType);
	MPI_Type_commit(&newType);

	// 2) Use MPI Sendrecv to perform a ring communication
	int dest = (rank + 1) % size;
	int source = (rank - 1 + size) % size;
	MPI_Sendrecv(sBuf, 1, newType, dest, 123, rBuf, 1, newType,
	             source, 123, MPI_COMM_WORLD, &status);

	MPI_Request send_req, recv_req;

	// 3) Use MPI Send and MPI Recv to perform a ring communication
	MPI_Isend(sBuf, 1, newType, dest, 456, MPI_COMM_WORLD, &send_req);
	MPI_Irecv(rBuf, 1, newType, source, 456, MPI_COMM_WORLD,
	    	&recv_req);

	MPI_Wait(&send_req, &status);
	MPI_Wait(&recv_req, &status);

	MPI_Type_free(&newType);

	// Say bye bye
	printf("Signing off, rank %d. \n", rank);

	MPI_Finalize();

	return 0;
}
