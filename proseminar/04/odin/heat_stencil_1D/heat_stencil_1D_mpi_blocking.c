#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

typedef double value_t;

#define RESOLUTION 120
#define HEAT_SOURCE_TEMP 60

// -- vector utilities --

typedef value_t *Vector;

Vector createVector(int N);

void releaseVector(Vector m);

void printTemperature(Vector m, int N);

int get_global_index(int num_elements, int rank, int remainder, int index) {
  return rank < remainder ? num_elements * rank : 
    (num_elements + 1) * remainder + (rank - remainder) * num_elements + index;

}

int from_global_index(int start, int global_index) {
  return global_index - start;
}

// blocking ghost cell exchange: every rank does the same two shifts, so each MPI_Sendrecv is matched
// by the neighbour's MPI_Sendrecv and nobody can deadlock. At the walls the neighbour is
// MPI_PROC_NULL, the call returns immediately and leaves the receive buffer untouched.
void exchange_ghost_cells(int left_rank, int right_rank, Vector local_A, int num_elements,
                          value_t *left_neighbor_cell_p, value_t *right_neighbor_cell_p) {
  // shift right: send rightmost cell to the right, receive the left neighbour's rightmost cell
  MPI_Sendrecv(&local_A[num_elements-1], 1, MPI_DOUBLE, right_rank, 42,
               left_neighbor_cell_p, 1, MPI_DOUBLE, left_rank, 42,
               MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  // shift left: send leftmost cell to the left, receive the right neighbour's leftmost cell
  MPI_Sendrecv(&local_A[0], 1, MPI_DOUBLE, left_rank, 43,
               right_neighbor_cell_p, 1, MPI_DOUBLE, right_rank, 43,
               MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

double new_temp(double tc, double tl, double tr) {
  return tc + 0.2 * (tl + tr + (-2 * tc));
}

// -- simulation code ---

int main(int argc, char **argv) {
  // 'parsing' optional input parameter = problem size
  MPI_Init(&argc, &argv);
  int N = 2000;
  if (argc > 1) {
    N = atoi(argv[1]);
  }
  int T = N * 500;
  if (argc > 2) {
    T = atoi(argv[2]);
  }


  // ---------- setup ----------


  int size, rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);



  int num_elements = N / size;

  if (num_elements < 1) {
    fprintf(stderr, "Each rank has only %d elements, needs at least 1\n", num_elements);
    MPI_Finalize();
    return EXIT_FAILURE;
  }
  int remainder = N % size;
  int source_x = N / 4;
  
  int *sendcounts = NULL;
  int *displs = NULL;

  // create a buffer for storing temperature fields only for root rank
  Vector A = NULL;

  if (rank == 0) {
    printf("Computing heat-distribution for room size N=%d for T=%d timesteps\n", N, T);
    A = createVector(N);

    // set up initial conditions in A
    for (int i = 0; i < N; i++) {
      A[i] = 273; // temperature is 0° C everywhere (273 K)
    }

    // and there is a heat source in one corner
    A[source_x] = 273 + HEAT_SOURCE_TEMP;
    
    sendcounts = malloc(size * sizeof(int));
    displs = malloc(size * sizeof(int));

    // similar to how MPI_scatterv is used in the documentation https://docs.open-mpi.org/en/v5.0.x/man-openmpi/man3/MPI_Scatterv.3.html
    // the size is just +1 for every rank before remainder
    int offset = 0;
    for (int i = 0; i < size; i++) {
        sendcounts[i] = N / size + (i < remainder);
        displs[i] = offset;
        offset += sendcounts[i];
    }
  }

  num_elements += rank < remainder;
  int start_index = get_global_index(num_elements, rank, remainder, 0);
  int end_index = start_index + num_elements;
  int has_heat_source = start_index <= source_x && source_x < end_index;
  int local_heat_source_x = from_global_index(start_index, source_x);

  // create a local buffer for storing temperature fields
  Vector local_A = createVector(num_elements);
  Vector local_B = createVector(num_elements);

  // use scatterv to send all the (non uniform) arrays out
  MPI_Scatterv(A, sendcounts, displs, MPI_DOUBLE,
     local_A, num_elements, MPI_DOUBLE, 0, MPI_COMM_WORLD);

  if (rank == 0) {
    printf("Initial:\t");
    printTemperature(A, N);
    printf("\n");
  }


  // ---------- compute ----------

  // create a second buffer for the computation

  int left_rank = rank == 0 ? MPI_PROC_NULL : rank-1;
  int right_rank = rank == size-1 ? MPI_PROC_NULL : rank+1;
  // for each time step ..
  for (int t = 0; t < T; t++) {
    // walls: a missing neighbour has the same temperature as the outermost cell
    value_t left_neighbor_cell = local_A[0];
    value_t right_neighbor_cell = local_A[num_elements-1];

    // send and receive outermost cell values, blocks until both have arrived
    exchange_ghost_cells(left_rank, right_rank, local_A, num_elements,
                         &left_neighbor_cell, &right_neighbor_cell);

    // .. we propagate the temperature
    for (long long i = 0; i < num_elements; i++) {
      // center stays constant (the heat is still on)
      if (has_heat_source && i == local_heat_source_x) {
        local_B[i] = local_A[i];
        continue;
      }

      // get temperature at current position
      value_t tc = local_A[i];

      // get temperatures of adjacent cells, the ghost cells at the local boundaries
      value_t tl = (i != 0) ? local_A[i - 1] : left_neighbor_cell;
      value_t tr = (i != num_elements - 1) ? local_A[i + 1] : right_neighbor_cell;

      // compute new temperature at current position
      local_B[i] = new_temp(tc, tl, tr);
    }

    // swap matrices (just pointers, not content)
    Vector local_H = local_A;
    local_A = local_B;
    local_B = local_H;

    if (!(t % 10000)) {
      //The gatherv is basically the oposite of the scatterv
      //just that the parameters are not in the same order
      //sendcounts is the same as receivecounts
      MPI_Gatherv(local_A, num_elements, MPI_DOUBLE, A, 
        sendcounts, displs, MPI_DOUBLE, 0, MPI_COMM_WORLD);
      // show intermediate step only rank 0
      if (rank == 0) {
        printf("Step t=%d:\t", t);
        printTemperature(A, N);
        printf("\n");
      }
    }
  }
  
  MPI_Gatherv(local_A, num_elements, MPI_DOUBLE, A, 
        sendcounts, displs, MPI_DOUBLE, 0, MPI_COMM_WORLD);

  releaseVector(local_B);
  releaseVector(local_A);

  // ---------- check ----------
  int success = 1;

  if (rank == 0) {
    printf("Final:\t\t");
    printTemperature(A, N);
    printf("\n");

    for (long long i = 0; i < N; i++) {
      value_t temp = A[i];
      if (273 <= temp && temp <= 273 + 60)
        continue;
      success = 0;
      break;
    }

    printf("Verification: %s\n", (success) ? "OK" : "FAILED");

    // ---------- cleanup ----------

    releaseVector(A);
    free(displs);
    free(sendcounts);
  }

  MPI_Bcast(&success, 1, MPI_INT, 0, MPI_COMM_WORLD);

  MPI_Finalize();



  // done
  return (success) ? EXIT_SUCCESS : EXIT_FAILURE;
}

Vector createVector(int N) {
  // create data and index vector
  return malloc(sizeof(value_t) * N);
}

void releaseVector(Vector m) { free(m); }

void printTemperature(Vector m, int N) {
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
  for (int i = 0; i < W; i++) {
    // get max temperature in this tile
    value_t max_t = 0;
    for (int x = sW * i; x < sW * i + sW; x++) {
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
