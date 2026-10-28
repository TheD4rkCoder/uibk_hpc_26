/* Minimal repro: receiver hard-codes sizeof(int)*2 bytes instead of
   using the derived type.  Works for 2 ints, truncates for 3 ints. */
#include <mpi.h>
#include <stdio.h>
int main(int argc, char **argv) {
    int rank;
    int sBuf[3] = { 1, 2, 3 }, rBuf[3] = { 0, 0, 0 };
    MPI_Datatype t;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Type_contiguous(3, MPI_INT, &t);
    MPI_Type_commit(&t);
    if (rank == 0)
        MPI_Send(sBuf, 1, t, 1, 1, MPI_COMM_WORLD);
    else
        MPI_Recv(rBuf, sizeof(int) * 2, MPI_BYTE, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    printf("rank %d: rBuf = {%d,%d,%d}\n", rank, rBuf[0], rBuf[1], rBuf[2]);
    MPI_Type_free(&t);
    MPI_Finalize();
    return 0;
}
