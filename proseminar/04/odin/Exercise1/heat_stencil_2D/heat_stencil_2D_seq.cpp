#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

typedef double value_t;

#define RESOLUTION 500

// edge length of the square tiles the grid is traversed in
#ifndef TILE
#define TILE 32
#endif

// -- matrix utilities --

// N x N matrix stored contiguously in row-major order: element (i,j) is at i*N+j
typedef std::vector<value_t> Matrix;

void printTemperature(const Matrix &m, int N);

// index of real cell (y,x) in the (N+2)x(N+2) padded grid; -1 and N address ghost cells
long long get_index(int y, int x, long long N) {
  return (N + 2) * (y + 1) + x + 1;
}

void fill_ghosts(Matrix &A, long long N);

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
  Matrix A((size_t)(N+2) * (N+2), 273);

  // and there is a heat source in one corner
  int source_x = N / 4;
  int source_y = N / 4;
  size_t source_index = get_index(source_y, source_x, N); 
  A[source_index] = 273 + 60;

  printf("Initial:\n");
  printTemperature(A, N);
  printf("\n");

  // ---------- compute ----------

  // create a second buffer for the computation
  Matrix B((size_t)(N+2) * (N+2));

  // for each time step ..
  for (int t = 0; t < T; t++) {
    fill_ghosts(A, N);

    // .. we propagate the temperature, tile by tile
    for (long long ii = 0; ii < N; ii += TILE) {
      for (long long jj = 0; jj < N; jj += TILE) {
        long long i_end = std::min<long long>(ii + TILE, N);
        long long j_end = std::min<long long>(jj + TILE, N);

        for (long long i = ii; i < i_end; i++) {
          for (long long j = jj; j < j_end; j++) {
            long long idx = get_index(i, j, N);

            

            // get temperature at current position
            value_t tc = A[idx];

            // get temperatures of adjacent cells
            value_t tu = A[idx - (N + 2)];
            value_t td = A[idx + (N + 2)];
            value_t tl = A[idx - 1];
            value_t tr = A[idx + 1];

            // compute new temperature at current position
            B[idx] = tc + 0.2 * (tl + tr + tu + td + (-4 * tc));
          }
        }
      }
    }
    // center stays constant (the heat is still on)
    B[source_index] = 273 + 60;

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

  // only check real cells, ghost cells (esp. the never-filled corners) are not part of the room
  int success = 1;
  for (int i = 0; i < N && success; i++) {
    for (int j = 0; j < N; j++) {
      value_t temp = A[get_index(i, j, N)];
      if (273 <= temp && temp <= 273 + 60)
        continue;
      success = 0;
      break;
    }
  }

  printf("Verification: %s\n", (success) ? "OK" : "FAILED");

  // done
  return (success) ? EXIT_SUCCESS : EXIT_FAILURE;
}

void fill_ghosts(Matrix &A, long long N) {
  //upper row
  for (int i = 0; i < N; i++) {
    A[get_index(-1, i, N)] = A[get_index(0, i, N)];
  }
  //lower row
  for (int i = 0; i < N; i++) {
    A[get_index(N, i, N)] = A[get_index(N-1, i, N)];
  }
  //left column
  for (int i = 0; i < N; i++) {
    A[get_index(i, -1, N)] = A[get_index(i, 0, N)];
  }
  //right column
  for (int i = 0; i < N; i++) {
    A[get_index(i, N, N)] = A[get_index(i, N-1, N)];
  }
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
      value_t max_t = m[get_index(sW * i, sW * j, N)];
      for (int y = sW * i; y < sW * (i + 1); y++) {
        for (int x = sW * j; x < sW * (j + 1); x++) {
          size_t idx = get_index(y, x, N);
          if (m[idx] > max_t) max_t = m[idx];
        }
      }
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
