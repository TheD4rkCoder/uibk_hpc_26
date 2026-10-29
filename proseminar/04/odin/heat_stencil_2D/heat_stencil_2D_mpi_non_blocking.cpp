#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>
#include <mpi.h>

typedef double value_t;

#define RESOLUTION 200

#define MPI_CHECK(call) do { \
  int err = (call); \
  if (err != MPI_SUCCESS) { \
    char errstr[MPI_MAX_ERROR_STRING]; \
    int len_; \
    MPI_Error_string(err,errstr, &len_); \
    int mpi_rank_ = -1; \
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank_); \
    std::fprintf(stderr, "[rank %d] MPI error at %s:%d: %.*s\n", \
      mpi_rank_, __FILE__, __LINE__, len_, errstr); \
    MPI_Abort(MPI_COMM_WORLD, err); \
    std::abort(); \
  } \
} while (0) \

// edge length of the square tiles the grid is traversed in
#ifndef TILE
#define TILE 32
#endif

// -- utilities --

enum Req {
  SEND_UP, RECV_DOWN,     // shift up
  SEND_DOWN, RECV_UP,     // shift down
  SEND_LEFT, RECV_RIGHT,  // shift left
  SEND_RIGHT, RECV_LEFT,  // shift right
  REQ_COUNT
};




void MPI_custom_Isendrecv(
  const void *sendbuf, 
  int sendcount,
  MPI_Datatype sendtype,
  int dest,
  int sendtag,
  void *recvbuf,
  int recvcount,
  MPI_Datatype recvtype,
  int source,
  int recvtag,
  MPI_Comm comm,
  MPI_Request *send_req,
  MPI_Request *recv_req) {
  
    MPI_CHECK(MPI_Isend(sendbuf,sendcount, sendtype, dest, sendtag, comm, send_req));
    MPI_CHECK(MPI_Irecv(recvbuf, recvcount, recvtype, source, recvtag, comm, recv_req));

}

// N x N matrix stored contiguously in row-major order: element (i,j) is at i*N+j
typedef std::vector<value_t> Matrix;

void printTemperature(const Matrix &m, int N);

// index of real cell (y,x) in the (N+2)x(N+2) padded grid; -1 and N address ghost cells
long long get_index(int y, int x, long long width) {
  return (width + 2) * (y + 1) + x + 1;
}

bool in_interval(int start, int end, int element) {
  return start <= element && element < end;
}

typedef struct {
  int up;
  int down;
  int left;
  int right;
} neighbors_t; 


// one stencil update of local cell (i,j): reads A (incl. ghost cells), writes B
inline void update_cell(const Matrix &A, Matrix &B, long long i, long long j, int local_cols) {
  long long idx = get_index(i, j, local_cols);

  // get temperature at current position
  value_t tc = A[idx];

  // get temperatures of adjacent cells
  value_t tu = A[idx - (local_cols + 2)];
  value_t td = A[idx + (local_cols + 2)];
  value_t tl = A[idx - 1];
  value_t tr = A[idx + 1];

  // compute new temperature at current position
  B[idx] = tc + 0.2 * (tl + tr + tu + td + (-4 * tc));
}

// the cells that read ghost cells: first/last row and first/last column of the local block.
// Must only run after MPI_Waitall, when the ghost cells have arrived. Every cell is updated
// exactly once, also for blocks that are only 1 or 2 cells wide/high.
void compute_outer_ring(const Matrix &A, Matrix &B, int local_rows, int local_cols) {
  // first and last row, including the corners
  for (int j = 0; j < local_cols; j++) {
    update_cell(A, B, 0, j, local_cols);
    if (local_rows > 1)
      update_cell(A, B, local_rows - 1, j, local_cols);
  }
  // first and last column, without the corners (already done above)
  for (int i = 1; i < local_rows - 1; i++) {
    update_cell(A, B, i, 0, local_cols);
    if (local_cols > 1)
      update_cell(A, B, i, local_cols - 1, local_cols);
  }
}

void fill_ghosts(Matrix &A, int local_cols, int local_rows, neighbors_t &neighbors,
                 MPI_Datatype column_t, MPI_Comm comm_cart, MPI_Request req[]);
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
  MPI_Init(&argc, &argv);

  int rank, num_ranks;
  MPI_CHECK(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
  MPI_CHECK(MPI_Comm_size(MPI_COMM_WORLD, &num_ranks));

  if (rank == 0)
    printf("Computing heat-distribution for room size N=%dx%d for T=%d timesteps\n", N, N, T);

  int dims[2] = {0,0};
  // let MPI handle all the annoying shit
  MPI_CHECK(MPI_Dims_create(num_ranks, 2, dims));

  // every rank computes the same dims, so all ranks take this branch together
  // if N is not suitable just fucking stop
  if (N % dims[0] != 0 || N % dims[1] != 0) {
    if (rank == 0)
      fprintf(stderr, "N=%d is not divisible by the %dx%d process grid (%d ranks)\n",
              N, dims[0], dims[1], num_ranks);
    MPI_Finalize();
    return EXIT_FAILURE;
  }

  int local_rows = N / dims[0];
  int local_cols = N / dims[1];
  
  // I found this great ressource which makes all this rather simple
  // https://philippgs.github.io/uibk_hpc_26/lecture/05_mpi_advanced/05_mpi_advanced.html#/creating-a-cartesian-topology
  int periods[2] = {0,0};
  MPI_Comm comm_cart;
  MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 1, &comm_cart);
  // Is MPI_PROC_NULL for edges
  int up, down, left, right;
  MPI_Cart_shift(comm_cart, 0, 1, &up, &down);
  MPI_Cart_shift(comm_cart, 1, 1, &left, &right);
  neighbors_t neighbors = {up, down, left, right};
  
  MPI_Request reqs[REQ_COUNT];
  
  MPI_CHECK(MPI_Comm_rank(comm_cart, &rank));

  // ---------- setup ----------

  // create a buffer for storing temperature fields
  // temperature is 0° C everywhere (273 K)

  Matrix local_A((size_t)(local_rows+2) * (local_cols+2),273);

  // one column of the local block: local_rows doubles, one padded row (local_cols+2) apart
  MPI_Datatype column_t;
  MPI_CHECK(MPI_Type_vector(local_rows, 1, local_cols + 2, MPI_DOUBLE, &column_t));
  MPI_CHECK(MPI_Type_commit(&column_t));

  int coords[2] = {0,0};
  MPI_CHECK(MPI_Cart_coords(comm_cart, rank, 2, coords));

  int global_row_start = coords[0]*local_rows;
  int global_row_end = global_row_start + local_rows;
  int global_column_start = coords[1]*local_cols;
  int global_column_end= global_column_start + local_cols;


  // and there is a heat source in one corner
  int source_x = N / 4;
  int source_y = N / 4;
  bool has_source = false;
  int source_index =  -1;
  if (in_interval(global_column_start, global_column_end, source_x) && 
      in_interval(global_row_start, global_row_end, source_y)) {
    has_source = true;
    int local_x = source_x - global_column_start;
    int local_y = source_y - global_row_start;
    source_index = get_index(local_y, local_x, local_cols);
    local_A[source_index] = 273 + 60;
  }

  /*printing tbd
  printf("Initial:\n");
  printTemperature(A, N);
  printf("\n");
  */
  // ---------- compute ----------

  // create a second buffer for the computation
  Matrix local_B((size_t)(local_rows+2) * (local_cols+2));

  // for each time step ..
  for (int t = 0; t < T; t++) {
    fill_ghosts(local_A, local_cols, local_rows, neighbors, column_t, comm_cart, reqs);

    // .. while the ghost cells are in flight, propagate the temperature of the inner cells
    // [1, local_rows-1) x [1, local_cols-1) tile by tile, they never read a ghost cell
    for (long long ii = 1; ii < local_rows - 1; ii += TILE) {
      for (long long jj = 1; jj < local_cols - 1; jj += TILE) {
        long long i_end = std::min<long long>(ii + TILE, local_rows - 1);
        long long j_end = std::min<long long>(jj + TILE, local_cols - 1);

        for (long long i = ii; i < i_end; i++) {
          for (long long j = jj; j < j_end; j++) {
            update_cell(local_A, local_B, i, j, local_cols);
          }
        }
      }
    }
    // Synchronize: ghost cells have arrived, the sent edge cells may be overwritten again
    MPI_CHECK(MPI_Waitall(REQ_COUNT, reqs, MPI_STATUSES_IGNORE));

    // now the cells that depend on the ghost cells
    compute_outer_ring(local_A, local_B, local_rows, local_cols);


    // center stays constant (the heat is still on)
    if (has_source) {
      local_B[source_index] = 273 + 60;
    }

    // swap matrices (just pointers, not content)
    std::swap(local_A, local_B);

  }

  // ---------- check ----------

  /*print tbd
  printf("Final:\n");
  printTemperature(local_A, N);
  printf("\n");
  */

  // only check real cells, ghost cells (esp. the never-filled corners) are not part of the room
  int success = 1;
  for (int i = 0; i < local_rows && success; i++) {
    for (int j = 0; j < local_cols; j++) {
      value_t temp = local_A[get_index(i, j, local_cols)];
      if (273 <= temp && temp <= 273 + 60)
        continue;
      success = 0;
      break;
    }
  }

  // int (not bool) to match MPI_INT; Allreduce so every rank returns the same exit code
  int agg_success;
  MPI_CHECK(MPI_Allreduce(&success, &agg_success, 1, MPI_INT, MPI_LAND, comm_cart));

  if (rank == 0) {
    printf("Verification: %s\n", (agg_success) ? "OK" : "FAILED");
  }

  MPI_CHECK(MPI_Type_free(&column_t));
  MPI_CHECK(MPI_Comm_free(&comm_cart));
  MPI_Finalize();

  // done
  return (agg_success) ? EXIT_SUCCESS : EXIT_FAILURE;
}

// Every rank does the same four shifts at the same time, so each Sendrecv is matched by the
// neighbour's Sendrecv. At the walls the neighbour is MPI_PROC_NULL: that send/receive is a
// no-op and the ghost cells are filled by copying the edge cells instead.
void fill_ghosts(Matrix &A, int local_cols, int local_rows, neighbors_t &neighbors,
                 MPI_Datatype column_t, MPI_Comm comm_cart, MPI_Request reqs[]) {
  //shift up: send top row up, receive lower ghost row from below
  MPI_custom_Isendrecv(
    &A[get_index(0, 0, local_cols)], local_cols, MPI_DOUBLE, neighbors.up, 42,
    &A[get_index(local_rows, 0, local_cols)], local_cols, MPI_DOUBLE, neighbors.down, 42,
    comm_cart, &reqs[SEND_UP], &reqs[RECV_DOWN]);

  //shift down: send bottom row down, receive upper ghost row from above
  MPI_custom_Isendrecv(
    &A[get_index(local_rows-1, 0, local_cols)], local_cols, MPI_DOUBLE, neighbors.down, 43,
    &A[get_index(-1, 0, local_cols)], local_cols, MPI_DOUBLE, neighbors.up, 43,
    comm_cart, &reqs[SEND_DOWN], &reqs[RECV_UP]);

  //shift left: send left column left, receive right ghost column from the right
  MPI_custom_Isendrecv(
    &A[get_index(0, 0, local_cols)], 1, column_t, neighbors.left, 44,
    &A[get_index(0, local_cols, local_cols)], 1, column_t, neighbors.right, 44,
    comm_cart, &reqs[SEND_LEFT], &reqs[RECV_RIGHT]);

  //shift right: send right column right, receive left ghost column from the left
  MPI_custom_Isendrecv(
    &A[get_index(0, local_cols-1, local_cols)], 1, column_t, neighbors.right, 45,
    &A[get_index(0, -1, local_cols)], 1, column_t, neighbors.left, 45,
    comm_cart, &reqs[SEND_RIGHT], &reqs[RECV_LEFT]);

  //upper row
  if (neighbors.up == MPI_PROC_NULL) {
    for (int i = 0; i < local_cols; i++) {
      A[get_index(-1, i, local_cols)]
        = A[get_index(0, i, local_cols)];
    }
  }
  //lower row
  if (neighbors.down == MPI_PROC_NULL) {
    for (int i = 0; i < local_cols; i++) {
      A[get_index(local_rows, i, local_cols)]
        = A[get_index(local_rows-1, i, local_cols)];
    }
  }
  //left column
  if (neighbors.left == MPI_PROC_NULL) {
    for (int i = 0; i < local_rows; i++) {
      A[get_index(i, -1, local_cols)]
        = A[get_index(i, 0, local_cols)];
    }
  }
  //right column
  if (neighbors.right == MPI_PROC_NULL) {
    for (int i = 0; i < local_rows; i++) {
      A[get_index(i, local_cols, local_cols)]
        = A[get_index(i, local_cols-1, local_cols)];
    }
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

