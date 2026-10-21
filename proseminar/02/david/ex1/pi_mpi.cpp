#include <mpi.h>

#include <iostream>
#include <cstdlib>
#include <random>
#include <chrono>

double pi_seq(long long N, int seed)
{
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> u(0.0, 1.0);
    long long hits = 0;
    // just a quarter of the circle is simpler with same results
    for (long long i = 0; i < N; ++i)
    {
        double x = u(rng), y = u(rng);
        if (x * x + y * y <= 1.0)
            ++hits;
    }
    return 4.0 * hits / static_cast<double>(N);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cout << "Usage: " << argv[0] << " <n>\n";
        return EXIT_FAILURE;
    }
    long long n = std::atoll(argv[1]);

    MPI_Init(&argc, &argv);
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size); // get the number of ranks
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); // get the rank of the calle
    // printf("Hello world from rank %d of %d\n", rank, size);

    // distribute the work evenly
    long long local_n = (n / size) + (rank < n % size ? 1 : 0);

    // sync start for best performance number :P
    MPI_Barrier(MPI_COMM_WORLD);
    double t0 = MPI_Wtime();

    double pi = pi_seq(local_n, rank);
    double total_pi;
    // rank 0 is the destination of the aggregated result
    MPI_Reduce(&pi, &total_pi, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    if (rank == 0)
    {
        total_pi /= size; // average over all ranks
    }

    double t1 = MPI_Wtime();

    if (rank == 0)
    {
        // std::printf("pi=%.10f  N=%lld\n", total_pi, n);
        // echo "N,ranks,pi,time_s" > "$csv"
        std::printf("%lld,%d,%.10f,%.6f\n", n, size, total_pi, t1 - t0);
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}