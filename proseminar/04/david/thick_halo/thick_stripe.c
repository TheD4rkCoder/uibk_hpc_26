/*
 * Quick and dirty: 1D stripes (rows only) with a HALO row deep halo that is
 * exchanged only every HALO steps instead of every step.
 *
 * The halo rows are recomputed as well, with a shrinking overlap: at sub-step
 * s only the ghost rows down to depth HALO-s are still valid, so that is what
 * gets computed.  Every row of my block still advances one step per step, so
 * any HALO works and the result is bitwise identical to the 1-row version --
 * the price is ~(HALO-1)/rows extra cell updates per step, the win is HALO
 * times fewer messages at the same number of bytes.  Two details keep it
 * bitwise identical to the 1-row version (bench.sh checks that): wall ranks
 * mirror the recomputed halo rows instead of computing them, and a
 * recomputed halo row holding the source cell gets clamped like the rest.
 *
 * usage: thick_stripe <n> <steps> [dump], output: n,steps,ranks,halo,time,OK
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHECK(call)                                                          \
    do                                                                       \
    {                                                                        \
        int err = (call);                                                    \
        if (err != MPI_SUCCESS)                                              \
        {                                                                    \
            fprintf(stderr, "%s:%d: MPI error %d\n", __FILE__, __LINE__, err); \
            MPI_Abort(MPI_COMM_WORLD, err);                                   \
        }                                                                    \
    } while (0)

#ifndef HALO
#define HALO 4
#endif

typedef double value_t;

/* mirror the walls, then swap the outermost `depth` rows with the neighbours */
static void exchange(value_t *buf, int rows, int cols, int depth, int up, int down)
{
    value_t *mine = buf + (size_t)HALO * cols;
    value_t *top = mine - (size_t)depth * cols;
    value_t *bot = mine + (size_t)rows * cols;

    for (int j = 1; j <= depth; j++)
    {
        if (up == MPI_PROC_NULL)
            memcpy(top + (size_t)(depth - j) * cols, mine + (size_t)(j - 1) * cols,
                   (size_t)cols * sizeof *mine);
        if (down == MPI_PROC_NULL)
            memcpy(bot + (size_t)(j - 1) * cols, mine + (size_t)(rows - j) * cols,
                   (size_t)cols * sizeof *mine);
    }
    CHECK(MPI_Sendrecv(mine, depth * cols, MPI_DOUBLE, up, 0,
                       bot, depth * cols, MPI_DOUBLE, down, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
    CHECK(MPI_Sendrecv(mine + (size_t)(rows - depth) * cols, depth * cols, MPI_DOUBLE, down, 1,
                       top, depth * cols, MPI_DOUBLE, up, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE));
}

/* rows r0..r1, 1 based for my block, 0 or negative = ghost rows above */
static void computeRows(value_t *nxt, const value_t *cur, int cols, int r0, int r1)
{
    const value_t alpha = 0.2;

    for (int r = r0; r <= r1; r++)
    {
        const value_t *c = cur + ((long long)r - 1) * cols;
        const value_t *u = cur + ((long long)r - 2) * cols;
        const value_t *d = cur + (long long)r * cols;
        value_t *out = nxt + ((long long)r - 1) * cols;

        out[0] = c[0] + alpha * (c[0] + c[1] + u[0] + d[0] - 4 * c[0]);
        for (int i = 1; i < cols - 1; i++)
            out[i] = c[i] + alpha * (c[i - 1] + c[i + 1] + u[i] + d[i] - 4 * c[i]);
        out[cols - 1] = c[cols - 1] + alpha * (c[cols - 2] + c[cols - 1] +
                                               u[cols - 1] + d[cols - 1] - 4 * c[cols - 1]);
    }
}

int main(int argc, char **argv)
{
    CHECK(MPI_Init(&argc, &argv));
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int n = 768, steps = 76800;
    if (argc > 1)
        n = atoi(argv[1]);
    if (argc > 2)
        steps = atoi(argv[2]);

    int rows = n / size + (rank < n % size);
    int first = rank * (n / size) + (rank < n % size ? rank : n % size);

    size_t cells = ((size_t)rows + 2 * HALO) * n;
    value_t *a = malloc(cells * sizeof *a);
    value_t *b = malloc(cells * sizeof *b);
    for (size_t i = 0; i < cells; i++)
        a[i] = b[i] = 273;

    int up = rank > 0 ? rank - 1 : MPI_PROC_NULL;
    int down = rank < size - 1 ? rank + 1 : MPI_PROC_NULL;
    int src_global = n / 4, src_col = n / 4;

    steps = (steps + HALO - 1) / HALO * HALO;

    struct timespec t0, t1;
    CHECK(MPI_Barrier(MPI_COMM_WORLD));
    clock_gettime(CLOCK_MONOTONIC, &t0);

    value_t *cur = a, *nxt = b;
    for (int t = 0; t < steps; t += HALO)
    {
        exchange(cur, rows, n, HALO, up, down);
        for (int s = 1; s <= HALO; s++)
        {
            int w = HALO - s; /* ghost depth that is still valid at this step */
            int r0 = 1 - w, r1 = rows + w;
            computeRows(nxt + (size_t)HALO * n, cur + (size_t)HALO * n, n, r0, r1);
            /* the source cell is clamped every step; a recomputed halo row must
             * be clamped too, or it disagrees with the neighbour that owns it */
            for (int r = r0; r <= r1; r++)
                if (first + r - 1 == src_global)
                    (nxt + (size_t)HALO * n)[((long long)r - 1) * n + src_col] = 273 + 60;
            /* wall ranks: mirror the recomputed halo instead of computing it, else
             * the last bits differ (different summation order) */
            value_t *nm = nxt + (size_t)HALO * n;
            for (int j = 1; j <= w; j++)
            {
                if (up == MPI_PROC_NULL)
                    memcpy(nm - (long long)j * n, nm + ((long long)j - 1) * n,
                           (size_t)n * sizeof *nm);
                if (down == MPI_PROC_NULL)
                    memcpy(nm + (size_t)(rows + j - 1) * n, nm + (size_t)(rows - j) * n,
                           (size_t)n * sizeof *nm);
            }
            value_t *tmp = cur;
            cur = nxt;
            nxt = tmp;
        }
    }

    CHECK(MPI_Barrier(MPI_COMM_WORLD));
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec - t0.tv_sec) + 1e-9 * (t1.tv_nsec - t0.tv_nsec);

    value_t final[2] = {1e30, -1e30};
    const value_t *mine = cur + (size_t)HALO * n;
    for (size_t i = 0; i < (size_t)rows * n; i++)
    {
        if (mine[i] < final[0])
            final[0] = mine[i];
        if (mine[i] > final[1])
            final[1] = mine[i];
    }
    double range[2];
    CHECK(MPI_Reduce(final, &range[0], 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD));
    CHECK(MPI_Reduce(&final[1], &range[1], 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD));

    if (rank == 0)
        printf("%d,%d,%d,%d,%.6f,%s\n", n, steps, size, HALO, elapsed,
               range[0] >= 273 && range[1] <= 333 ? "OK" : "FAILED");

    if (argc > 3)
    {
        int *counts = rank == 0 ? malloc(size * sizeof *counts) : NULL;
        int *displs = rank == 0 ? malloc(size * sizeof *displs) : NULL;
        CHECK(MPI_Gather(&rows, 1, MPI_INT, counts, 1, MPI_INT, 0, MPI_COMM_WORLD));
        value_t *full = NULL;
        if (rank == 0)
        {
            int offset = 0;
            full = malloc((size_t)n * n * sizeof *full);
            for (int r = 0; r < size; r++)
            {
                displs[r] = offset;
                offset += counts[r] * n;
                counts[r] *= n;
            }
        }
        CHECK(MPI_Gatherv(mine, rows * n, MPI_DOUBLE, full, counts, displs, MPI_DOUBLE, 0,
                          MPI_COMM_WORLD));
        if (rank == 0)
        {
            FILE *out = fopen(argv[3], "wb");
            fwrite(full, sizeof *full, (size_t)n * n, out);
            fclose(out);
            free(full);
            free(counts);
            free(displs);
        }
    }

    free(a);
    free(b);
    CHECK(MPI_Finalize());
    return 0;
}
