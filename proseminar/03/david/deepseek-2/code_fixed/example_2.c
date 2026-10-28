/*
 * Fixed version:
 *  - `(rank = 1 + size) % size` overwrote the local rank (undefined
 *    behaviour: `rank` is read and written in the same expression) and
 *    computed a bogus neighbour.  Use separate, read-only `dest` and
 *    `source` variables: source is the left neighbour (rank - 1 + size) % size.
 *  - send and receive datatypes now match: the receive side also uses
 *    one element of the contiguous datatype instead of 8 bytes of MPI_BYTE.
 *  - the blocking MPI_Send/MPI_Recv pair is replaced by a non-blocking
 *    Irecv/Isend pair + Waitall, so the ring cannot deadlock for large
 *    messages (classic ring deadlock).
 *  - the received data is actually checked.
 *  - the committed datatype is freed again.
 */

#include <mpi.h>
#include <stdio.h>

int main(int argc, char** argv) {
	int rank;
	int size;
	int sBuf[2] = { 1, 2 };
	int rBuf[2];
	MPI_Status status;
	MPI_Request reqs[2];
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

	// Right and left neighbour in the ring.
	const int dest = (rank + 1) % size;
	const int source = (rank - 1 + size) % size;

	// 2) Use MPI Sendrecv to perform a ring communication
	MPI_Sendrecv(sBuf, 1, newType, dest, 123, rBuf, 1, newType, source, 123,
	             MPI_COMM_WORLD, &status);
	if(rBuf[0] != 1 || rBuf[1] != 2) {
		fprintf(stderr, "rank %d: MPI_Sendrecv received wrong data (%d, %d)\n",
		        rank, rBuf[0], rBuf[1]);
	}

	// 3) Use MPI Send and MPI Recv to perform a ring communication
	//    Non-blocking first, otherwise two ranks can deadlock on the send.
	MPI_Irecv(rBuf, 1, newType, source, 456, MPI_COMM_WORLD, &reqs[0]);
	MPI_Isend(sBuf, 1, newType, dest, 456, MPI_COMM_WORLD, &reqs[1]);
	MPI_Waitall(2, reqs, MPI_STATUSES_IGNORE);
	if(rBuf[0] != 1 || rBuf[1] != 2) {
		fprintf(stderr, "rank %d: MPI_Send/MPI_Recv received wrong data (%d, %d)\n",
		        rank, rBuf[0], rBuf[1]);
	}

	MPI_Type_free(&newType);

	// Say bye bye
	printf("Signing off, rank %d. \n", rank);

	MPI_Finalize();

	return 0;
}
