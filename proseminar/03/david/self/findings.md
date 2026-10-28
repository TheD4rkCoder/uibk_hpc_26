after
`module load openmpi/3.1.6-gcc-12.2.0-d2gmn55`
I added these compiler flags:
`-g -Wall -Wextra -pedantic -fsanitize=address,undefined -O1`
and already got these warnings:
```
example_1.c: In function 'main':
example_1.c:13:9: warning: unused variable 'i' [-Wunused-variable]
   13 |     int i;
      |         ^
example_2.c: In function 'main':
example_2.c:32:28: warning: operation on 'rank' may be undefined [-Wsequence-point]
   32 |                      (rank = 1 + size) % size, 123, MPI_COMM_WORLD, &status);
      |                      ~~~~~~^~~~~~~~~~~
```
The sanitizers did not report any issues...
I removed the unused variable. And the `(rank = 1 + size) % size` makes no sense whatsoever. I just changed it to `(rank - 1 + size) % size`

To not execute on the login node, I ran interactive bash in a slurm job with:
`srun --pty --partition=lva --ntasks=1 --cpus-per-task=4 bash`
and `export OMPI_MCA_rmaps_base_oversubscribe=1` to allow -np 2 to run on the same node (just different cpu)

`mpiexec -np 2 ./example_1` (it hung) 

`mpiexec -np 2 ./example_2` (also hangs)

and then with
`/scratch/c703429/software/must-1.9.1/bin/mustrun --must:output stdout -np 2 ./example_1` (--output so that the output is not the default html file)
```
...
Emiting error code 3
[MUST-RUNTIME] ============MUST===============
[MUST-RUNTIME] ERROR: MUST detected a deadlock, detailed information is available in the MUST output file. You should either investigate details with a debugger or abort, the operation of MUST will stop from now.
[MUST-RUNTIME] ===============================
[MUST-REPORT] Error global: The application issued a set of MPI calls that can cause a deadlock! A graphical representation of this situation is available in a <a href="./MUST_Output-files/MUST_Deadlock.html" title="detailed deadlock view"> detailed deadlock view (./MUST_Output-files/MUST_Deadlock.html)</a>. References 1-1 list the involved calls (limited to the first 5 calls, further calls may be involved). The application still runs, if the deadlock manifested (e.g. caused a hang on this MPI implementation) you can attach to the involved ranks with a debugger or abort the application (if necessary). References of a representative process:
[MUST-REPORT] Reference 1: call MPI_Send@rank 0, threadid 0;
[MUST-REPORT] Stacktrace:
[MUST-REPORT] #0  main@/home/cb76/cb761152/uibk_hpc_26/proseminar/03/david/self/example_1.c:34
[MUST-REPORT]
```

so this line: `MPI_Send(&data, count, MPI_INT, i, tag, MPI_COMM_WORLD);`
is the problem. rank 0 sends to everyone, including itself, so i adjusted the loop to `for (i = 1; i < nprocs; i++)` and now it works.

Then I get:
```
[MUST-RUNTIME] ============MUST===============
[MUST-RUNTIME] ERROR: MUST detected a deadlock, 
...
[MUST-REPORT] Reference 1: call MPI_Send@rank 0, threadid 0;
[MUST-REPORT] Stacktrace:
[MUST-REPORT] #0  main@/home/cb76/cb761152/uibk_hpc_26/proseminar/03/david/self/example_1_copy.c:34
[MUST-REPORT] Reference 2: call MPI_Recv@rank 1, threadid 0;
[MUST-REPORT] Stacktrace:
[MUST-REPORT] #0  main@/home/cb76/cb761152/uibk_hpc_26/proseminar/03/david/self/example_1_copy.c:37
[MUST-REPORT]
```
The tags for send and receive are not the same...
```
[MUST-REPORT] MUST detected no MPI usage errors nor any suspicious behavior during this application run.
Emiting error code 0
```
now it seems fixed.

now with:
`/scratch/c703429/software/must-1.9.1/bin/mustrun -np 2 ./example_2` I got the following output:
```
[MUST-REPORT] Error global: The application issued a set of MPI calls that can cause a deadlock! ...
[MUST-REPORT] Reference 1: call MPI_Send@rank 0, threadid 0;
[MUST-REPORT] Stacktrace:
[MUST-REPORT] #0  __builtin_puts@/home/cb76/cb761152/uibk_hpc_26/proseminar/03/david/self/example_2_copy.c:35
[MUST-REPORT] Reference 2: call MPI_Send@rank 1, threadid 0;
[MUST-REPORT] Stacktrace:
[MUST-REPORT] #0  __builtin_puts@/home/cb76/cb761152/uibk_hpc_26/proseminar/03/david/self/example_2_copy.c:35
[MUST-REPORT] 
[MUST-REPORT] Error: from: call MPI_Type_contiguous@0-1: There are 1 datatypes that are not freed when MPI_Finalize was issued, a quality application should free all MPI resources before calling MPI_Finalize. Listing information for these datatypes:
[MUST-REPORT] 
[MUST-REPORT]  -Datatype 1: Datatype created at reference 1 is for C, commited at reference 2, based on the following type(s): { MPI_INT}
[MUST-REPORT]  Representative location:
[MUST-REPORT] #0  __builtin_puts@/home/cb76/cb761152/uibk_hpc_26/proseminar/03/david/self/example_2_copy.c:27
[MUST-REPORT]  References of a representative process:
[MUST-REPORT] Reference 1: call MPI_Type_contiguous@rank 1, threadid 0;
[MUST-REPORT] Stacktrace:
[MUST-REPORT] #0  __builtin_puts@/home/cb76/cb761152/uibk_hpc_26/proseminar/03/david/self/example_2_copy.c:27
[MUST-REPORT] Reference 2: call MPI_Type_commit@rank 1, threadid 0;
[MUST-REPORT] Stacktrace:
[MUST-REPORT] #0  __builtin_puts@/home/cb76/cb761152/uibk_hpc_26/proseminar/03/david/self/example_2_copy.c:28
[MUST-REPORT]
```
in this code: 
```c
	MPI_Send(sBuf, 1, newType, (rank + 1) % size, 456, MPI_COMM_WORLD);
	MPI_Recv(rBuf, sizeof(int) * 2, MPI_BYTE, (1 + rank) % size, 456, MPI_COMM_WORLD,
	         &status);
```
the send blocks, and everyone is waiting for the corresponding receive it is a deadlock. I replaced it with the async Isend and Irecv. (and added `MPI_Type_free(&newType);` at the end)


then I get this:
```
[MUST-REPORT] Error: from: call MPI_Irecv@1: A send and a receive operation use datatypes that do not match! Mismatch occurs at (contiguous)[0](MPI_INT) in the send type and at (MPI_BYTE) in the receive type (consult the MUST manual for a detailed description of datatype positions). The send operation was started at reference 1, the receive operation was started at reference 2. (Information on communicator: MPI_COMM_WORLD) (Information on send of count 1 with type:Datatype created at reference 3 is for C, commited at reference 4, based on the following type(s): { MPI_INT}) (Information on receive of count 8 with type:MPI_BYTE)
[MUST-REPORT]  Representative location:
[MUST-REPORT] #0  main@/home/cb76/cb761152/uibk_hpc_26/proseminar/03/david/self/example_2_copy.c:38
[MUST-REPORT]  References of a representative process:
[MUST-REPORT] Reference 1: call MPI_Isend@rank 0, threadid 0;
[MUST-REPORT] Stacktrace:
[MUST-REPORT] #0  main@/home/cb76/cb761152/uibk_hpc_26/proseminar/03/david/self/example_2_copy.c:37
[MUST-REPORT] Reference 2: call MPI_Irecv@rank 1, threadid 0;
[MUST-REPORT] Stacktrace:
[MUST-REPORT] #0  main@/home/cb76/cb761152/uibk_hpc_26/proseminar/03/david/self/example_2_copy.c:38
[MUST-REPORT] Reference 3: call MPI_Type_contiguous@rank 0, threadid 0;
[MUST-REPORT] Stacktrace:
[MUST-REPORT] #0  main@/home/cb76/cb761152/uibk_hpc_26/proseminar/03/david/self/example_2_copy.c:27
[MUST-REPORT] Reference 4: call MPI_Type_commit@rank 0, threadid 0;
[MUST-REPORT] Stacktrace:
[MUST-REPORT] #0  main@/home/cb76/cb761152/uibk_hpc_26/proseminar/03/david/self/example_2_copy.c:28
```

well the program creates a new own type but doesn't use it for the receive (instead uses MPI_BYTE)

`[MUST-REPORT] MUST detected no MPI usage errors nor any suspicious behavior during this application run.`

none of the programs check whether there are any errors in the return values of the MPI calls. I'm too lazy to add that.

Also what no debugger found is that the `&` here before `data` aren't needed (not sure why it works even with them)
```c
    if(myrank == 0) {
		for(int i = 1; i < nprocs; i++) {
			MPI_Send(&data, count, MPI_INT, i, tag, MPI_COMM_WORLD);
		}
	} else {
		MPI_Recv(&data, count, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);
	}
```

now let's see what AI can do: `deepseek-1/` and `deepseek-2/`