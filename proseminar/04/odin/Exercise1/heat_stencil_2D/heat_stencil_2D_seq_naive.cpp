// Benchmark baseline: flat row-major array, boundary/source checks in the inner loop, no tiling.

#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

typedef double value_t;

#define RESOLUTION 200

// -- matrix utilities --

// N x N matrix stored contiguously in row-major order: element (i,j) is at i*N+j
typedef std::vector<value_t> Matrix;

void printTemperature(const Matrix &m, int N);

// -- simulation code ---

int main(int argc, char **argv) {
  // 'parsing' optional input parameter = problem size
  int N = 768;
  if (argc > 1) {
    N = atoi(argv[1]);
  }
  int T = N * 100;
  if (argc > 2) {
    T = atoi(argv[2]);
  }
  printf("Computing heat-distribution for room size N=%dx%d for T=%d timesteps\n", N, N, T);

  // ---------- setup ----------

  // create a buffer for storing temperature fields
  // temperature is 0° C everywhere (273 K)
  Matrix A((size_t)N * N, 273);

  // and there is a heat source in one corner
  int source_x = N / 4;
  int source_y = N / 4;
  A[(size_t)source_y * N + source_x] = 273 + 60;

  printf("Initial:\n");
  printTemperature(A, N);
  printf("\n");

  // ---------- compute ----------

  // create a second buffer for the computation
  Matrix B((size_t)N * N);

  // for each time step ..
  for (int t = 0; t < T; t++) {
    // .. we propagate the temperature
    for (long long i = 0; i < N; i++) {
      for (long long j = 0; j < N; j++) {
        long long idx = i * N + j;

        // center stays constant (the heat is still on)
        if (i == source_y && j == source_x) {
          B[idx] = A[idx];
          continue;
        }

        // get temperature at current position
        value_t tc = A[idx];

        // get temperatures of adjacent cells
        value_t tu = (i != 0) ? A[idx - N] : tc;
        value_t td = (i != N - 1) ? A[idx + N] : tc;
        value_t tl = (j != 0) ? A[idx - 1] : tc;
        value_t tr = (j != N - 1) ? A[idx + 1] : tc;

        // compute new temperature at current position
        B[idx] = tc + 0.2 * (tl + tr + tu + td + (-4 * tc));
      }
    }

    // swap matrices (just pointers, not content)
    std::swap(A, B);

    // show intermediate step
    if (!(t % 10000)) {
      printf("Step t=%d:\n", t);
      printTemperature(A, N);
      printf("\n");
    }
  }

  // ---------- check ----------

  printf("Final:\n");
  printTemperature(A, N);
  printf("\n");

  int success = 1;
  for (value_t temp : A) {
    if (273 <= temp && temp <= 273 + 60)
      continue;
    success = 0;
    break;
  }

  printf("Verification: %s\n", (success) ? "OK" : "FAILED");

  // done
  return (success) ? EXIT_SUCCESS : EXIT_FAILURE;
}

void printTemperature(const Matrix &m, int N) {
  const char *colors = " .-:=+*^X#%@";
  const int numColors = 12;

  // boundaries for temperature (for simplicity hard-coded)
  const value_t T_MAX = 273 + 30;
  const value_t T_MIN = 273 + 0;

  // set the 'render' resolution (never more tiles than cells)
  int W = (N < RESOLUTION) ? N : RESOLUTION;

  // step size in each dimension
  int sW = N / W;

  // upper wall
  for (int j = 0; j < W + 2; j++)
    printf("X");
  printf("\n");

  for (int i = 0; i < W; i++) {
    // left wall
    printf("X");
    for (int j = 0; j < W; j++) {
      // get max temperature in this tile
      value_t max_t = m[(size_t)sW * i * N + sW * j];
      for (int y = sW * i; y < sW * (i + 1); y++)
        for (int x = sW * j; x < sW * (j + 1); x++)
          if (m[(size_t)y * N + x] > max_t) max_t = m[(size_t)y * N + x];

      // pick the 'color'
      int c = (int)(((max_t - T_MIN) / (T_MAX - T_MIN)) * numColors);
      c = (c >= numColors) ? numColors - 1 : (c < 0 ? 0 : c);
      printf("%c", colors[c]);
    }
    // right wall
    printf("X\n");
  }

  // lower wall
  for (int j = 0; j < W + 2; j++)
    printf("X");
}
