/*
 * This program does the same operation as an MPI_Bcast() but
 * does it using MPI_Send() and MPI_Recv().  We also time the
 * operation.
 *
 * Fixed version of example_1.c:
 *   - the receive tag matches the send tag (42, not 43);
 *   - rank 0 no longer sends to itself (nobody would receive it);
 *   - the send buffer is "data", not "&data";
 *   - "count" is an enum constant, so data[count] is a real array,
 *     and the shadowed/unused "int i" is gone.
 */

#include <mpi.h>
#include <stdio.h>

int main(int argc, char** argv) {
	int nprocs; /* the number of processes in the task */
	int myrank; /* my rank */
	enum { count = 10 };
	const int tag = 42; /* tag used for all communication */
	int good_data = 1;
	int data[count];    /* data buffers */
	MPI_Status status;  /* status of MPI_Recv() operation */

	MPI_Init(&argc, &argv);

	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

	for(int i = 0; i < count; i++)
		if(myrank == 0) {
			data[i] = 399; /* initialize the data for rank 0 process only.  */
		} else
			data[i] = i; /* initialize all other data buffers from 0 to count */

	if(myrank == 0) {
		for(int r = 1; r < nprocs; r++) /* rank 0 already has the data */
			MPI_Send(data, count, MPI_INT, r, tag, MPI_COMM_WORLD);
	} else {
		MPI_Recv(data, count, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);
	}

	/* Check the data everywhere. */

	if(myrank != 0) {
		for(int i = 0; i < count; i++) {
			if(data[i] != 399) {
				good_data = 0;
			}
		}
		if(good_data == 0) {
			fprintf(stdout, "Whoa! The data is incorrect\n");
		} else {
			fprintf(stdout, "Rank %d received correct data\n", myrank);
		}
	}

	MPI_Finalize();

	return good_data ? 0 : 1;
}
