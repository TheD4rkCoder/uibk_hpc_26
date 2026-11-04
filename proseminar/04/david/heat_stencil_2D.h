#ifndef HEAT_STENCIL_2D_H
#define HEAT_STENCIL_2D_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// -DFLOAT runs everything in single precision; the matching MPI type is selected
// in heat_stencil_2D.c
#ifdef FLOAT
typedef float value_t;
#else
typedef double value_t;
#endif

#define RESOLUTION 64

static inline value_t *createBuffer(size_t cells)
{
    value_t *buffer = malloc(cells * sizeof(value_t));
    if (!buffer)
    {
        fprintf(stderr, "out of memory\n");
        exit(EXIT_FAILURE);
    }
    return buffer;
}

static inline void releaseGrid(value_t *grid) { free(grid); }

// A grid carries one ghost layer on each side. Rows are padded to an even stride,
// so every row starts on a 16 byte boundary and the compiler can vectorize.
static inline int paddedStride(int cols) { return (cols + 3) & ~1; }

// One stencil update. The ghost layer takes care of walls and neighbours, so the
// loops have no branches and all programs use this same kernel
static inline void updateCell(value_t *restrict next, const value_t *restrict current,
                              int stride, int index)
{
    const value_t tc = current[index];
    // the factor must be a value_t: a plain 0.2 is double and would turn every
    // update into a double one
    const value_t alpha = 0.2;
    next[index] = tc + alpha * (current[index - 1] + current[index + 1] +
                                current[index - stride] + current[index + stride] - 4 * tc);
}

// Updates rows r0..r1 and columns c0..c1 (both inclusive, in grid coordinates where
// 0 and local+1 are the ghost layer).
static inline void computeRegion(value_t *restrict next, const value_t *restrict current,
                                 int stride, int r0, int r1, int c0, int c1)
{
    for (int r = r0; r <= r1; r++)
    {
        const int row = r * stride;
        int c = c0;
        // a loop that starts on an odd column cannot be vectorized, so the first
        // cell is peeled off if odd column/index (for memory alignment)
        if ((c & 1) != 0)
            updateCell(next, current, stride, row + c++);
        for (; c <= c1; c++)
            updateCell(next, current, stride, row + c);
    }
}

static inline double secondsSince(const struct timespec *start)
{
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    return (end.tv_sec - start->tv_sec) + (end.tv_nsec - start->tv_nsec) * 1e-9;
}

typedef struct
{
    int n, t_max, quiet;
    const char *dump_path;
} Options;

// Usage: [N] [T] [-q] [-d file]; T defaults to N * 100 (walls mirror/isolate)
static inline Options parseArguments(int argc, char **argv)
{
    Options o = {768, -1, 0, NULL};
    int positional = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-q") == 0)
            o.quiet = 1;
        else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc)
            o.dump_path = argv[++i];
        else if (positional == 0)
            o.n = atoi(argv[i]), positional++;
        else if (positional == 1)
            o.t_max = atoi(argv[i]), positional++;
        else
        {
            fprintf(stderr, "usage: %s [N] [T] [-q] [-d file]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (o.t_max <= 0)
        o.t_max = o.n * 100;
    if (o.n <= 0)
    {
        fprintf(stderr, "invalid N/T\n");
        exit(EXIT_FAILURE);
    }
    return o;
}

static inline void printTemperature(const value_t *grid, int n)
{
    static const char colors[] = " .-:=+*^X#%@";
    const int num_colors = 12;
    const value_t min = 273;
    const value_t max = 273 + 30;
    const int res = (n < RESOLUTION) ? n : RESOLUTION;
    const int tile = (n + res - 1) / res;

    for (int j = 0; j < res; j++)
    {
        const int y_end = ((j + 1) * tile > n) ? n : (j + 1) * tile;
        for (int i = 0; i < res; i++)
        {
            const int x_end = ((i + 1) * tile > n) ? n : (i + 1) * tile;
            value_t max_t = min;
            for (int y = j * tile; y < y_end; y++)
                for (int x = i * tile; x < x_end; x++)
                    if (max_t < grid[(size_t)y * n + x])
                        max_t = grid[(size_t)y * n + x];

            int c = (int)((max_t - min) / (max - min) * num_colors);
            c = (c >= num_colors) ? num_colors - 1 : ((c < 0) ? 0 : c);
            putchar(colors[c]);
        }
        putchar('\n');
    }
}

// nothing may leave the physical range of the room
static inline int checkRange(const value_t *grid, int n)
{
    for (long long i = 0; i < (long long)n * n; i++)
        if (!(grid[i] >= 273 && grid[i] <= 273 + 60))
            return 0;
    return 1;
}

// the grid as raw values, so that verify.sh can compare two runs
static inline void writeDump(const value_t *grid, int n, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f || fwrite(grid, sizeof(value_t), (size_t)n * n, f) != (size_t)n * n || fclose(f) != 0)
    {
        fprintf(stderr, "failed to write %s\n", path);
        exit(EXIT_FAILURE);
    }
}

// Final output; quiet mode prints one CSV line, which job.sh collects.
static inline int report(const Options *o, const value_t *grid, int ranks, double stencil, double total)
{
    const int success = checkRange(grid, o->n);

    if (!o->quiet)
    {
        value_t min_t = grid[0], max_t = grid[0];
        for (long long i = 1; i < (long long)o->n * o->n; i++)
        {
            if (min_t > grid[i])
                min_t = grid[i];
            if (max_t < grid[i])
                max_t = grid[i];
        }
        printf("Final:\n");
        printTemperature(grid, o->n);
        printf("Temperature range: %.6f .. %.6f K\n", min_t, max_t);
        printf("Verification: %s\n", success ? "OK" : "FAILED");
    }
    else
        printf("%d,%d,%.6f,%.6f,%s\n", o->n, ranks, stencil, total, success ? "OK" : "FAILED");

    if (o->dump_path)
        writeDump(grid, o->n, o->dump_path);

    return success;
}

#endif
