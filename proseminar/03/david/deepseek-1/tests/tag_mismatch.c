/* Minimal repro: sender uses tag 42, receiver waits on tag 43. */
#include <mpi.h>
#include <stdio.h>
int main(int argc, char **argv) {
    int rank, x = 7;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0)
        MPI_Send(&x, 1, MPI_INT, 1, 42, MPI_COMM_WORLD);
    else
        MPI_Recv(&x, 1, MPI_INT, 0, 43, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    printf("rank %d done\n", rank);
    MPI_Finalize();
    return 0;
}
