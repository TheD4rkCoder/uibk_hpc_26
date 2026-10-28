/* example_2 step 3 without the datatype/peer bugs: blocking Send then
 * Recv in a ring.  Works for 8 bytes, deadlocks once the message is
 * larger than the eager threshold. */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    int rank, size, n = 16 * 1024 * 1024;       /* 64 MiB */
    int *buf = malloc((size_t)n * sizeof *buf);
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    int next = (rank + 1) % size, prev = (rank + size - 1) % size;
#ifdef USE_ISEND
    MPI_Request req;
    MPI_Isend(buf, n, MPI_INT, next, 456, MPI_COMM_WORLD, &req);
    MPI_Recv(buf, n, MPI_INT, prev, 456, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Wait(&req, MPI_STATUS_IGNORE);
#else
    MPI_Send(buf, n, MPI_INT, next, 456, MPI_COMM_WORLD);
    MPI_Recv(buf, n, MPI_INT, prev, 456, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
#endif
    printf("rank %d completed ring\n", rank);
    free(buf);
    MPI_Finalize();
    return 0;
}
