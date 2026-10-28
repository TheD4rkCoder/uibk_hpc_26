/* Minimal repro of the peer/source expression in example_2:
 *   dest = (rank + 1) % size
 *   src  = (rank = 1 + size) % size
 * Two mistakes: '=' instead of '+', and '1 + size' instead of 'size - 1'.
 * The assignment also reads and writes rank in one argument list => UB.
 */
#include <stdio.h>

static int seen_dst, seen_src;
static void pair(int dst, int src) { seen_dst = dst; seen_src = src; }

int main(void) {
    printf("%4s %4s | %8s %8s | %6s %6s | %6s\n",
           "size", "rank", "next(dst)", "prev(src)", "dst", "src", "rank'");
    for (int size = 2; size <= 4; size++)
        for (int rank = 0; rank < size; rank++) {
            int r = rank;
            /* identical call shape and argument order to MPI_Sendrecv */
            pair((r + 1) % size, (r = 1 + size) % size);
            printf("%4d %4d | %8d %8d | %6d %6d | %6d\n",
                   size, rank, (rank + 1) % size, (rank + size - 1) % size,
                   seen_dst, seen_src, r);
        }
    printf("\nIn the working code 'dst' must equal next(dst) and 'src' must equal prev(src).\n"
           "GCC/x86-64 evaluates the second argument first, so 'dst' is corrupted too\n"
           "(that ordering is unspecified by C: -Wsequence-point flags the expression).\n");
    return 0;
}
