/* Is the MPI_BYTE/derived-type pairing itself safe? */
#include <mpi.h>
#include <stdio.h>
int main(int argc, char **argv) {
    int rank, size, s[2] = { 11, 22 }, r[2] = { -1, -1 };
    MPI_Datatype t;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Type_contiguous(2, MPI_INT, &t);
    MPI_Type_commit(&t);
    int next = (rank + 1) % size, prev = (rank + size - 1) % size;
#ifdef USE_MATCHING_TYPES
    MPI_Sendrecv(s, 1, t, next, 123, r, 1, t, prev, 123, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
#else
    MPI_Sendrecv(s, 1, t, next, 123, r, sizeof(int) * 2, MPI_BYTE, prev, 123,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
#endif
    printf("rank %d got {%d,%d}\n", rank, r[0], r[1]);
    MPI_Type_free(&t);
    MPI_Finalize();
    return 0;
}
