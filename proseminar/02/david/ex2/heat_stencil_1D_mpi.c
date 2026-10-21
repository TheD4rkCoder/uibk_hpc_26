#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// TODO: MPI_CHECK_ERROR macro or smth

typedef double value_t;

#define RESOLUTION 120
#define SYNC_SIZE 64

// -- vector utilities --

typedef value_t *Vector;

Vector createVector(int N);

void releaseVector(Vector m);

void printTemperature(Vector m, int N);

// -- simulation code ---

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    // 'parsing' optional input parameter = problem size
    int N = 2000;
    if (argc > 1)
    {
        N = atoi(argv[1]);
    }
    int quiet = 0;
    for (int i = 2; i < argc; i++)
        if (strcmp(argv[i], "-q") == 0)
            quiet = 1;
    int T = N * 500;

    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    // printf("Hello world from rank %d of %d\n", rank, size);

    int source_x = N / 4;
    int base = N / size, rem = N % size;
    int local_n = base + (rank < rem ? 1 : 0);
    int start = rank * base + (rank < rem ? rank : rem);
    int local_src = (source_x >= start && source_x < start + local_n)
                        ? source_x - start + SYNC_SIZE // +1 for padding
                        : -1;

    if (rank == 0 && !quiet)
        printf("Computing heat-distribution for room size N=%d for T=%d timesteps\n", N, T);

    // ---------- setup ----------

    // create a buffer for storing temperature fields
    Vector A = createVector(local_n + 2 * SYNC_SIZE);

    // set up initial conditions in A
    for (int i = SYNC_SIZE; i < local_n + SYNC_SIZE; i++)
    {
        A[i] = 273; // temperature is 0° C everywhere (273 K)
    }

    // and there is a heat source in one corner
    if (local_src >= 0)
        A[local_src] = 273 + 60;
    // if (rank == 0 && !quiet)
    // {
    //     printf("Initial:\t");
    //     printTemperature(A, local_n + 2 * SYNC_SIZE);
    //     printf("\n");
    // }
    // ---------- compute ----------

    // create a second buffer for the computation
    Vector B = createVector(local_n + 2 * SYNC_SIZE);

    Vector full_A = NULL;
    if (rank == 0)
    {
        full_A = createVector(N);
    }
    int *counts = NULL, *displs = NULL;
    if (rank == 0)
    {
        counts = malloc(size * sizeof(int));
        displs = malloc(size * sizeof(int));
        int d = 0;
        for (int r = 0; r < size; r++)
        {
            counts[r] = base + (r < rem ? 1 : 0); // how many elements
            displs[r] = d;                        // where to start placing the elements
            d += counts[r];
        }
    }

    // sync start for best performance number :P
    MPI_Barrier(MPI_COMM_WORLD);
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    // for each time step ..
    for (int t = 0; t < T;)
    {
        MPI_Request left_recv = MPI_REQUEST_NULL;
        MPI_Request right_recv = MPI_REQUEST_NULL;
        MPI_Request left_send = MPI_REQUEST_NULL;
        MPI_Request right_send = MPI_REQUEST_NULL;
        if (rank != 0)
        {
            MPI_Isend(&A[SYNC_SIZE], SYNC_SIZE, MPI_DOUBLE, rank - 1, 0, MPI_COMM_WORLD, &left_send);
            MPI_Irecv(&A[0], SYNC_SIZE, MPI_DOUBLE, rank - 1, 0, MPI_COMM_WORLD, &left_recv);
        }
        if (rank != size - 1)
        {
            MPI_Isend(&A[local_n], SYNC_SIZE, MPI_DOUBLE, rank + 1, 0, MPI_COMM_WORLD, &right_send);
            MPI_Irecv(&A[local_n + SYNC_SIZE], SYNC_SIZE, MPI_DOUBLE, rank + 1, 0, MPI_COMM_WORLD, &right_recv);
        }
        // TODO: move the waiting for the next data to the end of the loop
        // ensure data received
        MPI_Wait(&left_send, MPI_STATUS_IGNORE);
        MPI_Wait(&right_send, MPI_STATUS_IGNORE);
        MPI_Wait(&left_recv, MPI_STATUS_IGNORE);
        MPI_Wait(&right_recv, MPI_STATUS_IGNORE);

        // .. we propagate the temperature
        for (int j = 1; j < SYNC_SIZE + 1; j++, t++)
        {
            for (int i = j; i < local_n + 2 * SYNC_SIZE - j; i++)
            {
                // center stays constant (the heat is still on)
                if (i == local_src) // local_src is -1 if no heat source in this rank
                {
                    B[i] = A[i];
                    continue;
                }
                if (rank == 0)
                {
                    A[SYNC_SIZE - 1] = A[SYNC_SIZE];
                }

                if (rank == size - 1)
                {
                    A[local_n + SYNC_SIZE] = A[local_n + SYNC_SIZE - 1];
                }

                // get temperature at current position
                value_t tc = A[i];

                // get temperatures of adjacent cells
                value_t tl = A[i - 1];
                value_t tr = A[i + 1];

                // compute new temperature at current position
                B[i] = tc + 0.2 * (tl + tr + (-2 * tc));
            }
            Vector H = A;
            A = B;
            B = H;

            // swap matrices (just pointers, not content)

            // show intermediate step
            // if (rank == 0 && !quiet && !(t % 10000))
            // {
            //     printf("Step t=%d:\t", t);
            //     printTemperature(A, local_n + 2);
            //     printf("\n");
            // }
        }
    }

    // gather all the results to rank 0
    MPI_Gatherv(&A[SYNC_SIZE], local_n, MPI_DOUBLE,
                full_A, counts, displs, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;

    releaseVector(B);

    // ---------- check ----------
    if (rank == 0 && !quiet)
    {
        printf("Final:\t\t");
        printTemperature(full_A, N);
        printf("\n");
    }

    int success = 1;
    if (rank == 0)
    {
        for (long long i = 0; i < N; i++)
        {
            value_t temp = full_A[i];
            if (273 <= temp && temp <= 273 + 60)
                continue;
            success = 0;
            break;
        }
        if (!quiet)
            printf("Verification: %s\n", (success) ? "OK" : "FAILED");

        // CSV line (only in quiet mode, one row per run)
        // echo "N,ranks,time_s,success" > "$csv"
        if (quiet)
        {
            printf("%d,%d,%.6f,%s\n", N, size, elapsed, success ? "OK" : "FAILED");
            // print everything to compare with MPI version (for debugging)
            // for (long long i = 0; i < N; i++)
            // {
            //     printf("%.6f,", A[i]);
            // }
        }
    }

    // ---------- cleanup ----------            printf("%d,0,%.6f,%s\n", N, elapsed, success ? "OK" : "FAILED");

    releaseVector(A);
    if (rank == 0)
    {
        releaseVector(full_A);
        free(counts);
        free(displs);
    }

    MPI_Finalize();
    // done
    return (success) ? EXIT_SUCCESS : EXIT_FAILURE;
}

Vector createVector(int N)
{
    // create data and index vector
    return malloc(sizeof(value_t) * N);
}

void releaseVector(Vector m) { free(m); }

void printTemperature(Vector m, int N)
{
    const char *colors = " .-:=+*^X#%@";
    const int numColors = 12;

    // boundaries for temperature (for simplicity hard-coded)
    const value_t max = 273 + 30;
    const value_t min = 273 + 0;

    // set the 'render' resolution
    int W = RESOLUTION;

    // step size in each dimension
    int sW = N / W;

    // room
    // left wall
    printf("X");
    // actual room
    for (int i = 0; i < W; i++)
    {
        // get max temperature in this tile
        value_t max_t = 0;
        for (int x = sW * i; x < sW * i + sW; x++)
        {
            max_t = (max_t < m[x]) ? m[x] : max_t;
        }
        value_t temp = max_t;

        // pick the 'color'
        int c = ((temp - min) / (max - min)) * numColors;
        c = (c >= numColors) ? numColors - 1 : ((c < 0) ? 0 : c);

        // print the average temperature
        printf("%c", colors[c]);
    }
    // right wall
    printf("X");
}
