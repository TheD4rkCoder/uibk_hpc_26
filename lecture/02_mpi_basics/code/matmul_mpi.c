#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#define SIZE 4

int A[SIZE][SIZE];
int B[SIZE][SIZE];
int C[SIZE][SIZE];

void fill_matrix(int m[SIZE][SIZE]);
void print_matrix(int m[SIZE][SIZE]);

int main(int argc, char** argv) {
	int myRank, numProcs;
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
	MPI_Comm_size(MPI_COMM_WORLD, &numProcs);

	// if matrix size not divisible
	if(SIZE % numProcs != 0) {
		MPI_Finalize();
		return EXIT_FAILURE;
	}

	// root generates input data
	if(myRank == 0) {
		fill_matrix(A);
		fill_matrix(B);
	}

	// compute boundaries of local computation
	int start = myRank * SIZE / numProcs;
	int end = (myRank + 1) * SIZE / numProcs;

	// distribute rows of A to everyone
	MPI_Scatter(A, SIZE * SIZE / numProcs, MPI_INT, A[start], SIZE * SIZE / numProcs, MPI_INT, 0,
	            MPI_COMM_WORLD);

	// send entire matrix B to everyone
	MPI_Bcast(B, SIZE * SIZE, MPI_INT, 0, MPI_COMM_WORLD);

	// local computation of every rank
	for(int i = start; i < end; i++) {
		for(int j = 0; j < SIZE; j++) {
			C[i][j] = 0;
			for(int k = 0; k < SIZE; k++) {
				C[i][j] += A[i][k] * B[k][j];
			}
		}
	}

	// gather result rows back to root
	MPI_Gather(C[start], SIZE * SIZE / numProcs, MPI_INT, C, SIZE * SIZE / numProcs, MPI_INT, 0,
	           MPI_COMM_WORLD);

	if(myRank == 0) {
		print_matrix(C);
	}

	MPI_Finalize();
	return EXIT_SUCCESS;
}

void fill_matrix(int m[SIZE][SIZE]) {
	for(int i = 0; i < SIZE; i++) {
		for(int j = 0; j < SIZE; j++) {
			m[i][j] = i + j;
		}
	}
}

void print_matrix(int m[SIZE][SIZE]) {
	for(int i = 0; i < SIZE; i++) {
		for(int j = 0; j < SIZE; j++) {
			printf("%4d ", m[i][j]);
		}
		printf("\n");
	}
}
