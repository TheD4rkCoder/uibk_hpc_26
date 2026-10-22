# Exercise 1

The first program does a manual `MPI_Bcast` with `MPI_Send` and `MPI_Recv`. 
The second one is supposedly used to do like 2 ring communications of the same data. It is not very clear, IMO.

## Compiler Warnings

I just added the standard `-Wall -Wextra -pedantic`. These are a collection of very common errors that could occour.

Output:
```
example_1.c: In function ‘main’:
example_1.c:13:9: warning: unused variable ‘i’ [-Wunused-variable]
   13 |     int i;
      |         ^
example_2.c: In function ‘main’:
example_2.c:32:28: warning: operation on ‘rank’ may be undefined [-Wsequence-point]
   32 |                      (rank = 1 + size) % size, 123, MPI_COMM_WORLD, &status);
      |              
```

The first one is pretty clear. At the beginning `int i` gets declared but everytime an i is used, it is inside a for loop which does the initialisation of the loop variable i itself (like any sane person would do). FIX: remove the declaration at the beginning

The second one regards this function call in example_2.c

```c
MPI_Sendrecv(sBuf, 1, newType, (rank + 1) % size, 123, rBuf, sizeof(int) * 2, MPI_BYTE,
            (rank = 1 + size) % size, 123, MPI_COMM_WORLD, &status);
```

More specifically the 2 uses of rank. Since the order in which the function prameters get evaluated is not specified, it is unclear wether the rank should first be set to 1 + size and then used in rank+1 or the other way around. (https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html)
This is why it does not show up for the following part:

```c
MPI_Send(sBuf, 1, newType, (rank + 1) % size, 456, MPI_COMM_WORLD);
MPI_Recv(rBuf, sizeof(int) * 2, MPI_BYTE, (rank = 1 + size) % size, 456, MPI_COMM_WORLD,
            &status);
```

Since here it the order is clear. The "fix" for example to add a `int dest = (rank + 1) % size` variable.
(I am currently ignoring the fact that the assignment does not make sense at all)

SUCESS! THE COMPILER WARNINGS ARE GONE

## Debugger

Since when executing both programs, they lead to an infinite loop, i postponed the sanitizer and started with must. I ran the must_submit.sh jobscript and I got the following output for both programs:

```
[MUST-RUNTIME] ============MUST===============
[MUST-RUNTIME] ERROR: MUST detected a deadlock, detailed information is available in the MUST output file. You should either investigate details with a debugger or abort, the operation of MUST will stop from now.
[MUST-RUNTIME] ===============================
```

OH NO.

So next tried to use gdb. For some reason it did not work on LCC3, which is why I did this on my machine.

### example_1

I started the program with `mpiexec -n 2 ./example_1` and looked for both processes with ps.
Then I attached gdb to each one. One was stuck at the barrier, and the other one was stuck at the receive in line 36.

This is of course because the tags of the send and receive need to match.
Now the program finishes.

### example_2
Here the gdb tells me, that it one process is stuck at the sendrecv and the other at the second receive.
This is obviously because the sending pattern doesn't make any sense at all lol.
The destination is correct but the source is like so weird. I fixed it by making the source `int source = (rank - 1 + size) % size`, which is the left neighboor with a wraparound to size-1 for rank = 0;
The same problem obviously applies to the manual send and receive. Where i just added the same source and destination.





## Sanitizer

I added -fsanitize=undefined

