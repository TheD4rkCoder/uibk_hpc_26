# Exercise 1

## Implementation

The implementation is really simple. First the rank 0 calculates the number of samples every rank has to simulate and the rest, in case the number of samples is not divisible by the number of samples. Then it broadcasts that number to every rank. The 0 rank then adds the rest to its number of samples. Every rank does the sampling and at the end there is a sum reduction for the the number of samples in the circles. 

I realised a bit too late, that the first broadcast is completely useless, everybody can just calculate the samples own its own and only rank 0 adds the remainder. If i have time to implement I'll do that

## Results

### comparing ranks for n=10⁹
So when looking at the time for samples=10⁹ we can see that for ranks=1 the sequential implementation is actually faster by almost a factor 2, because the mpi overhead. For 2 ranks the time is about the same and after that there is a nice linear decline in execution time. This is because the problem is embarassingly parallel. For n=96 we get a speedup of 19.3x.
![](./Exercise1/plots/mpi_time_vs_tasks.png)

| ranks | sequential time [s] | mpi time [s] | speedup vs seq |
| --- | ---: | ---: | ---: |
| seq | 27.406000 | - | 1.000 |
| 1 | 27.406000 | 46.439000 | 0.590 |
| 2 | 27.406000 | 26.162000 | 1.048 |
| 4 | 27.406000 | 12.356000 | 2.218 |
| 8 | 27.406000 | 6.539000 | 4.191 |
| 16 | 27.406000 | 3.869000 | 7.083 |
| 32 | 27.406000 | 2.371000 | 11.559 |
| 64 | 27.406000 | 1.658000 | 16.530 |
| 96 | 27.406000 | 1.420000 | 19.300 |

### comparing sequential to max ranks
We can see that for smaller n the sequential implementation is faster. After 10⁷ samples the execution time rises dramatically, while the parallel version stays really stable and scales well.
![](./Exercise1/plots/seq_vs_mpi96_time.png)

### Error to pi
I mean this version to approximate pi is really bad. It is computationally expensive and does not a good job at approximating it. but there is not really a significant difference in implementation, when it comes to error. Interestingly it seems that with more samples the accuracy goes down after 10⁷ for mpi and after 10⁸ for sequential. I dont know if this is actually there is numeric reason behind it. Maybe fp precision error or problems with the rng
![](./Exercise1/plots/seq_vs_mpi96_error.png)

---
AI notice:
The python scripts as well as the parsing for the csv file in the slurm scripts was done with the help of GPT


# Exercise 2