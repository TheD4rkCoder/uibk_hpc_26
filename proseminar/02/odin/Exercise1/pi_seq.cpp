// Code inspired by https://www.geeksforgeeks.org/dsa/estimating-value-pi-using-monte-carlo/

#include <bits/stdc++.h>

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
    long long circle_points = 0;
    double rand_x, rand_y, origin_dist;

    // Initializing rand()
    srand(42);
    long long n = atoll(argv[1]);

    // Total Random numbers generated = possible x
    // values * possible y values
    for (long long i = 0; i < n; i++) {

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

    // estimated pi 
    double pi = double(4 * circle_points) / n;

    // Final Estimated Value
    cout << "\nFinal Estimation of Pi = " << pi << endl;

    return 0;
}