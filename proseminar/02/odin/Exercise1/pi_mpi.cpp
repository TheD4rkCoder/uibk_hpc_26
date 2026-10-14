// Code inspired by https://www.geeksforgeeks.org/dsa/estimating-value-pi-using-monte-carlo/

#include <bits/stdc++.h>
#include <mpi.h>

// Defines precision for x and y values. More the
// interval, more the number of significant digits
#define INTERVAL 10000
using namespace std;

int main(int argc, char *argv[])
{
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <number of points>" << endl;
        return 1;
    }
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    long long circle_points = 0;
    double rand_x, rand_y, origin_dist;

    // Initializing rand()
    srand(rank);
    long long n = atoll(argv[1]);
    long long individual_n;
    long long rest;

    if (rank == 0) {
        individual_n = n / size;
        rest = n % size;
    }
    MPI_Bcast(&individual_n, 1, MPI_LONG_LONG, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        individual_n += rest;
    }

    // Total Random numbers generated = possible x
    // values * possible y values
    for (long long i = 0; i < individual_n; i++) {

        // Randomly generated x and y values
        rand_x = double(rand() % (INTERVAL + 1)) / INTERVAL;
        rand_y = double(rand() % (INTERVAL + 1)) / INTERVAL;

        // Distance between (x, y) from the origin
        origin_dist = rand_x * rand_x + rand_y * rand_y;

        // Checking if (x, y) lies inside the define
        // circle with R=1
        if (origin_dist <= 1)
            circle_points++;

    }

    long long total_circle_points;

    MPI_Reduce(&circle_points, &total_circle_points, 1, 
        MPI_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);


    if (rank == 0) {
        // estimated pi 
        double pi = double(4 * total_circle_points) / n;

        // Final Estimated Value
        cout << "\nFinal Estimation of Pi = " << pi << endl;
    }

    MPI_Finalize();

    return 0;
}