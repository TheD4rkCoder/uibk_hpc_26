/*
 * This program does the same operation as an MPI_Bcast() but
 * does it using MPI_Send() and MPI_Recv().  We also time the
 * operation.
 */

#include <mpi.h>
#include <stdio.h>

#define COUNT 10

int main(int argc, char** argv) {
	int nprocs; /* the number of processes in the task */
	int myrank; /* my rank */
	int tag = 42;  /* tag used for all communication */
	int good_data = 1;
	int data[COUNT];   /* data buffers */
	MPI_Status status; /* status of MPI_Recv() operation */
	double start, end;

	MPI_Init(&argc, &argv);

	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

	for(int i = 0; i < COUNT; i++)
		if(myrank == 0) {
			data[i] = 399; /* initialize the data for rank 0 process only.  */
		} else
			data[i] = i; /* initialize all other data buffers from 0 to count */

	MPI_Barrier(MPI_COMM_WORLD);
	start = MPI_Wtime();

	if(myrank == 0) {
		/* rank 0 already owns the data, so it must not send to itself */
		for(int i = 1; i < nprocs; i++) {
			MPI_Send(data, COUNT, MPI_INT, i, tag, MPI_COMM_WORLD);
		}
	} else {
		MPI_Recv(data, COUNT, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);
	}

	MPI_Barrier(MPI_COMM_WORLD);
	end = MPI_Wtime();

	/* Check the data everywhere. */

	if(myrank != 0) {
		for(int i = 0; i < COUNT; i++) {
			if(data[i] != 399) {
				good_data = 0;
			}
		}
		if(good_data == 0) {
			fprintf(stdout, "Whoa! The data is incorrect\n");
		} else {
			fprintf(stdout, "Rank %d received correct data\n", myrank);
		}
	} else {
		fprintf(stdout, "Broadcast to %d processes took %f s\n", nprocs, end - start);
	}

	MPI_Finalize();

	return 0;
}
