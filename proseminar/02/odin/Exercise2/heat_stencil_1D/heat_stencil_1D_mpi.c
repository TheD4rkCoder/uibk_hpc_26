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

void Isend_and_Ireceive_right(int rank, value_t *outermost_cell_right_p, value_t *neighbor_cell_right_p,
                             MPI_Request *right_send_p, MPI_Request *right_recv_p) {
      MPI_Isend(outermost_cell_right_p, 1, MPI_DOUBLE, rank+1, 42, MPI_COMM_WORLD, right_send_p);
      MPI_Irecv(neighbor_cell_right_p, 1, MPI_DOUBLE, rank+1, 42, MPI_COMM_WORLD, right_recv_p);
}

void Isend_and_Ireceive_left(int rank, value_t *outermost_cell_left_p, value_t *neighbor_cell_left_p,
                            MPI_Request *left_send_p, MPI_Request *left_recv_p) {
      MPI_Isend(outermost_cell_left_p, 1, MPI_DOUBLE, rank-1, 42, MPI_COMM_WORLD, left_send_p);
      MPI_Irecv(neighbor_cell_left_p, 1, MPI_DOUBLE, rank-1, 42, MPI_COMM_WORLD, left_recv_p);
}

double new_temp(double tc, double tl, double tr) {
  return tc + 0.2 * (tl + tr + (-2 * tc));
}

// -- simulation code ---

int main(int argc, char **argv) {
  // 'parsing' optional input parameter = problem size
  int N = 2000;
  if (argc > 1) {
    N = atoi(argv[1]);
  }
  int T = N * 500;
  printf("Computing heat-distribution for room size N=%d for T=%d timesteps\n", N, T);

  // ---------- setup ----------

  MPI_Init(&argc, &argv);

  int size, rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);



  int num_elements = N / size;

  if (num_elements < 4) {
    fprintf(stderr, "Each rank has only %d elements, needs at least 4", num_elements);
    return EXIT_FAILURE;
  }
  int remainder = N % size;
  int source_x = N / 4;
  
  int *sendcounts, *displs;

  // create a buffer for storing temperature fields only for root rank
  Vector A;

  if (rank == 0) {
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
  int has_heat_source = start_index < source_x && source_x < end_index;
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

  // for each time step ..
  for (int t = 0; t < T; t++) {
    // send and receive outermost cell value
    MPI_Request left_recv,right_recv, left_send, right_send;
    value_t left_neighbor, right_neighbor;
    if (rank == 0) {
      // edging left, just send and receive right
      Isend_and_Ireceive_right(rank, &local_A[num_elements-1], 
        &right_neighbor, &right_send, &right_recv);
    }
    else if (rank == size-1) {
      //edging right, just send and receive left
      Isend_and_Ireceive_left(rank, &local_A[0],
        &left_neighbor, &left_send, &left_recv);
    }
    else {
      //not edging send and receive both sides
      Isend_and_Ireceive_right(rank, &local_A[num_elements-1], 
        &left_neighbor, &right_send, &right_recv);
      Isend_and_Ireceive_left(rank, &local_A[0],
        &left_neighbor, &left_send, &left_recv);
    }

    // .. we propagate the temperature
    // first 2 and last 2 cells have to be done outside the loop
    // because local_A[0] and local_A[number_elements] shouldn't be read from with Isend
    // and it is better to do the wait at the end for latency hiding
    // this results in the minimum num_elements size being 4 (could be reduced to 3 with a couple more ifs)
    for (long long i = 2; i < num_elements-2; i++) {
      // center stays constant (the heat is still on)
      if (has_heat_source && i == local_heat_source_x) {
        local_B[i] = local_A[i];
        continue;
      }

      // get temperature at current position
      value_t tc = local_A[i];

      // get temperatures of adjacent cells
      value_t tl = local_A[i - 1];
      value_t tr = local_A[i + 1];

      // compute new temperature at current position
      local_B[i] = new_temp(tc, tl, tr);
    }

    // get right and left neighboor
    if (rank == 0) {
      // Wait for rightmost neighboor message and calculate the 2 leftmost
      local_B[0] = new_temp(local_A[0], local_A[0], local_A[1]);
      local_B[1] = new_temp(local_A[1], local_A[0], local_A[2]);
      MPI_Wait(&right_recv, MPI_STATUS_IGNORE);
      MPI_Wait(&right_send, MPI_STATUS_IGNORE);
      local_B[num_elements-2] = new_temp(local_A[num_elements-2], local_A[num_elements-3], local_A[num_elements-1]);
      local_B[num_elements-1] = new_temp(local_A[num_elements-1], local_A[num_elements-2], right_neighbor);
    }
    else if (rank == size-1) {
      // calculate the 2 rightmost and wait for the 2 leftmost
      local_B[num_elements-2] = new_temp(local_A[num_elements-2], local_A[num_elements-3], local_A[num_elements-1]);
      local_B[num_elements-1] = new_temp(local_A[num_elements-1], local_A[num_elements-2], local_A[num_elements-1]);
      MPI_Wait(&left_recv, MPI_STATUS_IGNORE);
      MPI_Wait(&left_send, MPI_STATUS_IGNORE);
      local_B[0] = new_temp(local_A[0], left_neighbor, local_A[1]);
      local_B[1] = new_temp(local_A[1], local_A[0], local_A[2]);
    }
    else {
      //wait for both sides and then calculate
      MPI_Wait(&left_recv, MPI_STATUS_IGNORE);
      MPI_Wait(&left_send, MPI_STATUS_IGNORE);
      MPI_Wait(&right_recv, MPI_STATUS_IGNORE);
      MPI_Wait(&right_send, MPI_STATUS_IGNORE);
      local_B[0] = new_temp(local_A[0], left_neighbor, local_A[1]);
      local_B[1] = new_temp(local_A[1], local_A[0], local_A[2]);
      local_B[num_elements-2] = new_temp(local_A[num_elements-2], local_A[num_elements-3], local_A[num_elements-1]);
      local_B[num_elements-1] = new_temp(local_A[num_elements-1], local_A[num_elements-2], right_neighbor);
    }

    // This is because with a lot of ranks it is not unlikely that the heat source falls exactly
    // on the boundary of the local array
    // the comparisons are cheap because of branch prediction the real "issue" is that
    // we make a useless receive and send
    // counteracting this, is a bit too annyoing for me rn ngl
    if (has_heat_source) {
      if (local_heat_source_x == 0) {
        local_B[0] = 273 + HEAT_SOURCE_TEMP;
      }
      if (local_heat_source_x == num_elements-1) {
        local_B[num_elements-1] = 273 + HEAT_SOURCE_TEMP;
      }
    }




    


    // swap matrices (just pointers, not content)
    Vector local_H = local_A;
    local_A = local_B;
    local_B = local_H;

    //The gatherv is basically the oposite of the scatterv
    //just that the parameters are not in the same order
    //sendcounts is the same as receivecounts
    MPI_Gatherv(local_A, num_elements, MPI_DOUBLE, local_A, 
      sendcounts, displs, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    // show intermediate step only rank 0
    if (rank == 0 && !(t % 10000)) {
      printf("Step t=%d:\t", t);
      printTemperature(A, N);
      printf("\n");
    }
  }

  releaseVector(local_B);
  releaseVector(local_A);

  // ---------- check ----------

  if (rank == 0) {
    printf("Final:\t\t");
    printTemperature(A, N);
    printf("\n");
  }

  int success = 1;
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
