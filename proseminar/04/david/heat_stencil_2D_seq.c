#include <time.h>

#include "heat_stencil_2D.h"

int main(int argc, char **argv)
{
    struct timespec t_start;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    const Options o = parseArguments(argc, argv);
    const int n = o.n;
    const int src_y = n / 4, src_x = n / 4;
    const int stride = paddedStride(n);
    const size_t cells = (size_t)(n + 2) * stride;

    value_t *a = createBuffer(cells);
    value_t *b = createBuffer(cells);
    for (size_t i = 0; i < cells; i++)
        a[i] = 273, b[i] = 273;
    a[(size_t)(src_y + 1) * stride + src_x + 1] = 273 + 60;

    if (!o.quiet)
        printf("Computing heat-distribution for room size N=%dx%d for T=%d timesteps\n", n, n, o.t_max);

    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int t = 0; t < o.t_max; t++)
    {
        // isolated walls: mirror the border rows and columns into the ghost layer
        // TODO: skip the memcpy by checking an `if` in the inner loop / have the if outside and have multiple loops
        // bottom and top
        memcpy(&a[1], &a[stride + 1], n * sizeof(value_t));
        memcpy(&a[(size_t)(n + 1) * stride + 1], &a[(size_t)n * stride + 1], n * sizeof(value_t));
        for (int y = 1; y <= n; y++)
        {
            // left and right (bad for cache but whatever)
            a[(size_t)y * stride] = a[(size_t)y * stride + 1];
            a[(size_t)y * stride + n + 1] = a[(size_t)y * stride + n];
        }

        computeRegion(b, a, stride, 1, n, 1, n);

        b[(size_t)(src_y + 1) * stride + src_x + 1] = 273 + 60;

        value_t *swap = a;
        a = b;
        b = swap;
    }

    const double stencil = secondsSince(&t0);

    // compact copy of the inner cells, without the ghost layer
    value_t *inner = createBuffer((size_t)n * n);
    for (int y = 0; y < n; y++)
        memcpy(&inner[(size_t)y * n], &a[(size_t)(y + 1) * stride + 1], n * sizeof(value_t));

    const int success = report(&o, inner, 0, stencil, secondsSince(&t_start));

    releaseGrid(inner);
    releaseGrid(a);
    releaseGrid(b);

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
