#include <mpi.h>
#include <time.h>

#include "heat_stencil_2D.h"

#ifdef FLOAT
#define MPI_VALUE_T MPI_FLOAT
#else
#define MPI_VALUE_T MPI_DOUBLE
#endif

// call is the return value of some MPI function
#define MPI_CHECK_ERROR(call)                                                \
    do                                                                       \
    {                                                                        \
        const int rc = (call);                                               \
        if (rc != MPI_SUCCESS)                                               \
        {                                                                    \
            char text[MPI_MAX_ERROR_STRING];                                 \
            int len = 0;                                                     \
            MPI_Error_string(rc, text, &len);                                \
            fprintf(stderr, "%s:%d: %.*s\n", __FILE__, __LINE__, len, text); \
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);                         \
        }                                                                    \
    } while (0)

typedef struct
{
    int n, rank, size;
    int rank_rows, rank_cols, my_row, my_col;
    int start_row, start_col, local_rows, local_cols, stride;
    int up, down, left, right; // MPI_PROC_NULL on the walls of the room
    int src_r, src_c;          // heat source in local coordinates, -1 if not on this rank
    value_t *a, *b;
    // The halo columns are packed into contiguous buffers: a strided MPI type would
    // send one value per row and is far slower.
    value_t *snd_l, *snd_r, *rcv_l, *rcv_r;
} Domain;

static void *xmalloc(size_t bytes)
{
    void *p = malloc(bytes);
    if (!p)
    {
        fprintf(stderr, "out of memory\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    return p;
}

// The walls of the room are isolated, so the ghost layer mirrors the border.
static void setMirrors(Domain *d)
{
    value_t *a = d->a;

    if (d->up == MPI_PROC_NULL)
        memcpy(&a[1], &a[d->stride + 1], d->local_cols * sizeof(value_t));
    if (d->down == MPI_PROC_NULL)
        memcpy(&a[(d->local_rows + 1) * d->stride + 1], &a[d->local_rows * d->stride + 1],
               d->local_cols * sizeof(value_t));
    if (d->left == MPI_PROC_NULL)
        for (int r = 1; r <= d->local_rows; r++)
            a[r * d->stride] = a[r * d->stride + 1];
    if (d->right == MPI_PROC_NULL)
        for (int r = 1; r <= d->local_rows; r++)
            a[r * d->stride + d->local_cols + 1] = a[r * d->stride + d->local_cols];
}

#ifndef NOCOMM

// Copies the border columns into the send buffers and back into the ghost columns.
static void packColumns(const Domain *d)
{
    for (int r = 1; r <= d->local_rows; r++)
    {
        d->snd_l[r - 1] = d->a[r * d->stride + 1];
        d->snd_r[r - 1] = d->a[r * d->stride + d->local_cols];
    }
}

static void unpackColumns(Domain *d)
{
    // on the wall there is nothing to unpack, the ghost layer keeps the mirrored value
    if (d->left != MPI_PROC_NULL)
        for (int r = 1; r <= d->local_rows; r++)
            d->a[r * d->stride] = d->rcv_l[r - 1];
    if (d->right != MPI_PROC_NULL)
        for (int r = 1; r <= d->local_rows; r++)
            d->a[r * d->stride + d->local_cols + 1] = d->rcv_r[r - 1];
}

#endif

#ifdef BLOCKING

// Blocks until the halo is complete: two round trips, nothing overlaps the compute.
static void exchangeHalo(Domain *d)
{
    value_t *a = d->a;
    const int stride = d->stride, rows = d->local_rows, cols = d->local_cols;

    MPI_CHECK_ERROR(MPI_Sendrecv(&a[stride + 1], cols, MPI_VALUE_T, d->up, 0,
                                 &a[1], cols, MPI_VALUE_T, d->up, 0,
                                 MPI_COMM_WORLD, MPI_STATUS_IGNORE));
    MPI_CHECK_ERROR(MPI_Sendrecv(&a[rows * stride + 1], cols, MPI_VALUE_T, d->down, 0,
                                 &a[(rows + 1) * stride + 1], cols, MPI_VALUE_T, d->down, 0,
                                 MPI_COMM_WORLD, MPI_STATUS_IGNORE));

    packColumns(d);
    MPI_CHECK_ERROR(MPI_Sendrecv(d->snd_l, rows, MPI_VALUE_T, d->left, 0,
                                 d->rcv_l, rows, MPI_VALUE_T, d->left, 0,
                                 MPI_COMM_WORLD, MPI_STATUS_IGNORE));
    MPI_CHECK_ERROR(MPI_Sendrecv(d->snd_r, rows, MPI_VALUE_T, d->right, 0,
                                 d->rcv_r, rows, MPI_VALUE_T, d->right, 0,
                                 MPI_COMM_WORLD, MPI_STATUS_IGNORE));
    unpackColumns(d);
}

#elif !defined(NOCOMM)

// Starts the halo exchange and leaves the requests to the caller.
static void startExchange(Domain *d, MPI_Request req[8])
{
    value_t *a = d->a;
    const int stride = d->stride, rows = d->local_rows, cols = d->local_cols;
    int i = 0;

    MPI_CHECK_ERROR(MPI_Irecv(&a[1], cols, MPI_VALUE_T, d->up, 0,
                              MPI_COMM_WORLD, &req[i++]));
    MPI_CHECK_ERROR(MPI_Isend(&a[stride + 1], cols, MPI_VALUE_T, d->up, 0,
                              MPI_COMM_WORLD, &req[i++]));
    MPI_CHECK_ERROR(MPI_Irecv(&a[(rows + 1) * stride + 1], cols, MPI_VALUE_T, d->down, 0,
                              MPI_COMM_WORLD, &req[i++]));
    MPI_CHECK_ERROR(MPI_Isend(&a[rows * stride + 1], cols, MPI_VALUE_T, d->down, 0,
                              MPI_COMM_WORLD, &req[i++]));

    packColumns(d);
    MPI_CHECK_ERROR(MPI_Irecv(d->rcv_l, rows, MPI_VALUE_T, d->left, 0,
                              MPI_COMM_WORLD, &req[i++]));
    MPI_CHECK_ERROR(MPI_Isend(d->snd_l, rows, MPI_VALUE_T, d->left, 0,
                              MPI_COMM_WORLD, &req[i++]));
    MPI_CHECK_ERROR(MPI_Irecv(d->rcv_r, rows, MPI_VALUE_T, d->right, 0,
                              MPI_COMM_WORLD, &req[i++]));
    MPI_CHECK_ERROR(MPI_Isend(d->snd_r, rows, MPI_VALUE_T, d->right, 0,
                              MPI_COMM_WORLD, &req[i++]));
}

#endif

// Splits the room into one block per rank; the first ranks get the extra rows/columns.
static void setupDomain(Domain *d, int n, int rank, int size)
{
    int dims[2] = {0, 0};
#ifdef STRIPES
    dims[0] = size; // one horizontal stripe per rank: rows only, no column split
    dims[1] = 1;
#else
    // let MPI choose how to make `size` ranks into a square-ish x*y grid;
    // Handles awkward sizes etc for me: size = 7 -> {7, 1} or {1, 7}; size = 12 -> {4, 3}
    // May consult the network topology on some MPI implementations
    MPI_CHECK_ERROR(MPI_Dims_create(size, 2, dims));
#endif
    const int rank_rows = dims[0], rank_cols = dims[1];
    if (n < rank_rows || n < rank_cols)
    {
        if (rank == 0)
            fprintf(stderr, "N=%d too small for a %dx%d rank grid\n", n, rank_rows, rank_cols);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    d->n = n;
    d->rank = rank;
    d->size = size;
    d->rank_rows = rank_rows;
    d->rank_cols = rank_cols;
    d->my_row = rank / rank_cols;
    d->my_col = rank % rank_cols;

    const int row_base = n / rank_rows, row_rem = n % rank_rows;
    const int col_base = n / rank_cols, col_rem = n % rank_cols;
    d->local_rows = row_base + (d->my_row < row_rem);
    d->local_cols = col_base + (d->my_col < col_rem);
    d->start_row = d->my_row * row_base + (d->my_row < row_rem ? d->my_row : row_rem);
    d->start_col = d->my_col * col_base + (d->my_col < col_rem ? d->my_col : col_rem);
    d->stride = paddedStride(d->local_cols);

    const int src_y = n / 4, src_x = n / 4;
    const int has_source = src_y >= d->start_row && src_y < d->start_row + d->local_rows &&
                           src_x >= d->start_col && src_x < d->start_col + d->local_cols;
    d->src_r = has_source ? src_y - d->start_row + 1 : -1;
    d->src_c = has_source ? src_x - d->start_col + 1 : -1;

    d->up = (d->my_row > 0) ? rank - rank_cols : MPI_PROC_NULL;
    d->down = (d->my_row < rank_rows - 1) ? rank + rank_cols : MPI_PROC_NULL;
    d->left = (d->my_col > 0) ? rank - 1 : MPI_PROC_NULL;
    d->right = (d->my_col < rank_cols - 1) ? rank + 1 : MPI_PROC_NULL;

    const size_t cells = (size_t)(d->local_rows + 2) * d->stride;
    d->a = xmalloc(cells * sizeof(value_t));
    d->b = xmalloc(cells * sizeof(value_t));
    for (size_t i = 0; i < cells; i++)
        d->a[i] = 273, d->b[i] = 273;
    if (has_source)
        d->a[d->src_r * d->stride + d->src_c] = 273 + 60;

    d->snd_l = xmalloc(d->local_rows * sizeof(value_t));
    d->snd_r = xmalloc(d->local_rows * sizeof(value_t));
    d->rcv_l = xmalloc(d->local_rows * sizeof(value_t));
    d->rcv_r = xmalloc(d->local_rows * sizeof(value_t));
}

int main(int argc, char **argv)
{
    MPI_CHECK_ERROR(MPI_Init(&argc, &argv));

    struct timespec t_start;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    int rank, size;
    MPI_CHECK_ERROR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    MPI_CHECK_ERROR(MPI_Comm_size(MPI_COMM_WORLD, &size));

    const Options o = parseArguments(argc, argv);
    const int n = o.n;

    Domain d;
    setupDomain(&d, n, rank, size);

    if (rank == 0 && !o.quiet)
        printf("Computing heat-distribution for room size N=%dx%d for T=%d timesteps on %d ranks (%dx%d)\n",
               n, n, o.t_max, size, d.rank_rows, d.rank_cols);

    struct timespec t0;
    MPI_CHECK_ERROR(MPI_Barrier(MPI_COMM_WORLD)); // will slow down the total time, but whatever
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int t = 0; t < o.t_max; t++)
    {
        setMirrors(&d);

#if defined(NOCOMM)
        // no communication at all: same decomposition, wrong result, lower bound
        computeRegion(d.b, d.a, d.stride, 1, d.local_rows, 1, d.local_cols);
#elif defined(BLOCKING)
        exchangeHalo(&d);
        computeRegion(d.b, d.a, d.stride, 1, d.local_rows, 1, d.local_cols);
#else
        MPI_Request req[8];
        startExchange(&d, req);

        // the inner cells do not read the halo, so they overlap the exchange
        computeRegion(d.b, d.a, d.stride, 2, d.local_rows - 1, 2, d.local_cols - 1);

        // then wait for the halo and compute the frame around it
        MPI_CHECK_ERROR(MPI_Waitall(8, req, MPI_STATUSES_IGNORE));
        unpackColumns(&d);
        computeRegion(d.b, d.a, d.stride, 1, 1, 1, d.local_cols);                           // top
        computeRegion(d.b, d.a, d.stride, d.local_rows, d.local_rows, 1, d.local_cols);     // bottom
        computeRegion(d.b, d.a, d.stride, 2, d.local_rows - 1, 1, 1);                       // left
        computeRegion(d.b, d.a, d.stride, 2, d.local_rows - 1, d.local_cols, d.local_cols); // right
#endif

        if (d.src_r >= 0) // -1 means the source is not on this rank
            d.b[d.src_r * d.stride + d.src_c] = 273 + 60;

        value_t *swap = d.a;
        d.a = d.b;
        d.b = swap;
    }

    // still slows down the total time, but idc (negligible)
    MPI_CHECK_ERROR(MPI_Barrier(MPI_COMM_WORLD));
    const double stencil_time_s = secondsSince(&t0);

    // rank 0 collects the blocks; they are row-major, so the offsets are known
    value_t *block = xmalloc((size_t)d.local_rows * d.local_cols * sizeof(value_t));
    for (int r = 0; r < d.local_rows; r++)
        memcpy(&block[(size_t)r * d.local_cols], &d.a[(r + 1) * d.stride + 1],
               d.local_cols * sizeof(value_t));

    value_t *packed = NULL, *full = NULL;
    int *counts = NULL, *displs = NULL;
    if (rank == 0)
    {
        packed = xmalloc((size_t)n * n * sizeof(value_t));
        full = createBuffer((size_t)n * n);
        counts = xmalloc(size * sizeof(int));
        displs = xmalloc(size * sizeof(int));
        for (int r = 0; r < size; r++)
        {
            const int rr = r / d.rank_cols, rc = r % d.rank_cols;
            const int rows = n / d.rank_rows + (rr < n % d.rank_rows);
            const int cols = n / d.rank_cols + (rc < n % d.rank_cols);
            counts[r] = rows * cols;
            displs[r] = (r == 0) ? 0 : displs[r - 1] + counts[r - 1];
        }
    }
    MPI_CHECK_ERROR(MPI_Gatherv(block, d.local_rows * d.local_cols, MPI_VALUE_T,
                                packed, counts, displs, MPI_VALUE_T, 0, MPI_COMM_WORLD));

    if (rank == 0)
    {
        for (int r = 0; r < size; r++)
        {
            const int rr = r / d.rank_cols, rc = r % d.rank_cols;
            const int rows_base = n / d.rank_rows, rows_rem = n % d.rank_rows;
            const int cols_base = n / d.rank_cols, cols_rem = n % d.rank_cols;
            const int rows = rows_base + (rr < rows_rem), cols = cols_base + (rc < cols_rem);
            const int first_row = rr * rows_base + (rr < rows_rem ? rr : rows_rem);
            const int first_col = rc * cols_base + (rc < cols_rem ? rc : cols_rem);
            for (int y = 0; y < rows; y++)
                memcpy(&full[(size_t)(first_row + y) * n + first_col],
                       &packed[displs[r] + (size_t)y * cols], cols * sizeof(value_t));
        }
    }

    const int success = (rank == 0) ? report(&o, full, size, stencil_time_s, secondsSince(&t_start)) : 1;

    free(d.snd_l);
    free(d.snd_r);
    free(d.rcv_l);
    free(d.rcv_r);
    free(d.a);
    free(d.b);
    free(block);
    if (rank == 0)
    {
        free(packed);
        releaseGrid(full);
        free(counts);
        free(displs);
    }

    MPI_CHECK_ERROR(MPI_Finalize());
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
