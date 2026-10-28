/*
 * Fixed version of example_2.c:
 *   - "next" and "prev" are computed from rank with +, not with an
 *     assignment; the old "(rank = 1 + size) % size" both overwrote rank
 *     (undefined behaviour) and always evaluated to 1;
 *   - the receive datatype is the same derived type as the send datatype;
 *   - the ring send is non-blocking, so the blocking MPI_Recv cannot
 *     deadlock with the neighbouring rank's blocking MPI_Send;
 *   - the received data is checked, and the derived type is freed.
 */

#include <mpi.h>
#include <stdio.h>

int main(int argc, char** argv) {
	int rank;
	int size;
	int sBuf[2] = { 1, 2 };
	int rBuf[2] = { -1, -1 };
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

	// 1) Create a datatype
	MPI_Type_contiguous(2, MPI_INT, &newType);
	MPI_Type_commit(&newType);

	int next = (rank + 1) % size;
	int prev = (rank + size - 1) % size;

	// 2) Use MPI Sendrecv to perform a ring communication
	MPI_Sendrecv(sBuf, 1, newType, next, 123, rBuf, 1, newType, prev, 123,
	             MPI_COMM_WORLD, &status);
	printf("rank %d: Sendrecv got {%d,%d} from rank %d\n", rank, rBuf[0], rBuf[1], prev);

	// 3) Use MPI Send and MPI Recv to perform a ring communication.
	//    The send must not block: a blocking Send followed by Recv can
	//    deadlock once the message is larger than the eager buffer.
	MPI_Request req;
	MPI_Isend(sBuf, 1, newType, next, 456, MPI_COMM_WORLD, &req);
	MPI_Recv(rBuf, 1, newType, prev, 456, MPI_COMM_WORLD, &status);
	MPI_Wait(&req, MPI_STATUS_IGNORE);
	printf("rank %d: Send/Recv got {%d,%d} from rank %d\n", rank, rBuf[0], rBuf[1], prev);

	// Say bye bye
	if(rBuf[0] != sBuf[0] || rBuf[1] != sBuf[1]) {
		printf("rank %d: wrong data received!\n", rank);
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
	printf("Signing off, rank %d. \n", rank);

	MPI_Type_free(&newType);
	MPI_Finalize();

	return 0;
}
