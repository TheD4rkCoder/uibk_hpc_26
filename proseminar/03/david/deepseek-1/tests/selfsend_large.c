/* rank 0 sends to itself and never receives (example_1 does this with
   every rank, including rank 0).  Small messages are buffered, large
   ones block: the same code path is a latent deadlock. */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    int rank, n = 16 * 1024 * 1024;             /* 64 MiB */
    int *buf = malloc((size_t)n * sizeof *buf);
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (rank == 0) {
        MPI_Send(buf, n, MPI_INT, 0, 42, MPI_COMM_WORLD);
        printf("self-send returned\n");
    }
    free(buf);
    MPI_Finalize();
    return 0;
}
