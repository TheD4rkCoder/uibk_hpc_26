/*
 * This program does the same operation as an MPI_Bcast() but
 * does it using MPI_Send() and MPI_Recv().  We also time the
 * operation.
 *
 * Fixed version:
 *  - rank 0 no longer sends to itself (nobody would receive it and
 *    rank 0 would then block in MPI_Barrier)
 *  - send and receive use the same tag (was 42 vs. tag2 == 43)
 *  - unused variable `i` removed
 */

#include <mpi.h>
#include <stdio.h>

int main(int argc, char** argv) {
	int nprocs; /* the number of processes in the task */
	int myrank; /* my rank */
	const int count = 10;
	const int tag = 42; /* tag used for all communication */
	int good_data = 1;
	int data[count];    /* data buffers */
	MPI_Status status;  /* status of MPI_Recv() operation */

	MPI_Init(&argc, &argv);

	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

	if(nprocs < 2) {
		fprintf(stderr, "This test needs at least 2 processes!\n");
		MPI_Finalize();
		return 1;
	}

	for(int i = 0; i < count; i++)
		if(myrank == 0) {
			data[i] = 399; /* initialize the data for rank 0 process only.  */
		} else
			data[i] = i; /* initialize all other data buffers from 0 to count */

	if(myrank == 0) {
		/* Send to everybody except myself: rank 0 does not receive. */
		for(int i = 1; i < nprocs; i++) {
			MPI_Send(data, count, MPI_INT, i, tag, MPI_COMM_WORLD);
		}
	} else {
		/* Must use the same tag the sender used. */
		MPI_Recv(data, count, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);
	}

	MPI_Barrier(MPI_COMM_WORLD);

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

	return 0;
}
