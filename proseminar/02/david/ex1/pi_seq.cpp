#include <mpi.h> // for timing

#include <cstdio>
#include <cstdlib>
#include <random>
#include <chrono>

double pi_seq(long long N)
{
    std::mt19937_64 rng(42);
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
        printf("Usage: %s <n>\n", argv[0]);
        return EXIT_FAILURE;
    }
    long long n = std::atoll(argv[1]);

    double t0 = MPI_Wtime();
    double pi = pi_seq(n);
    double t1 = MPI_Wtime();
    // echo "N,ranks,pi,time_s" > "$csv" 
    // ranks=0 is the sequential version
    std::printf("%lld,%d,%.10f,%.6f\n", n, 0, pi, t1 - t0);
}