# Exercise 2 — AI-assisted MPI debugging

## 1. Tool and prompts

**AI tool:** DeepSeek via the DeepSeek Harness (DSH) Web GUI, model `deepseek-flash`,
running as an autonomous coding/debugging agent on the local machine.

Prompt that started the task (verbatim):

> Complete exercise 2 inside `../../readme.md`. The code is in @../../code/ .
> In the end write in a file what bugs and issues you found, with what tools you
> found them, and how you fixed them. Just ask whenever I should install anything.

Follow-up prompt (the agent asked before installing anything, as instructed):

> **Agent:** No MPI is installed on this machine. May I install OpenMPI purely
> inside my workspace (download Fedora RPMs with dnf, extract with rpm2cpio, no
> root, no system changes)? […] Which additional debug tools may I set up locally […]?
>
> **User:** "Yes, install system-wide (I'll give sudo)" and, for tools,
> "gdb (already installed), gcc ASan/UBSan, valgrind via local RPM extraction —
> Whatever you think is reasonable. I'll install whatever you want."

`sudo` turned out to be impossible in this sandbox (`The "no new privileges" flag
is set`), so the agent fell back to the approved extraction route:

* MPICH 4.2.2 + `mpicc`/`mpiexec` (RPMs downloaded with `dnf download --resolve
  --arch=x86_64`, unpacked with `rpm2cpio | cpio -idm` into `tools/prefix_mpich`);
* valgrind 3.27.1, `libasan`/`libubsan` (same extraction);
* environment is set up by `tools/env_mpich.sh` (PATH, `LD_LIBRARY_PATH`, include
  path, `VALGRIND_LIB`). Nothing outside the session workspace was modified.

OpenMPI 5 was extracted too, but its PRRTE runtime has hard-coded `/usr/share`
paths that cannot be relocated without root, so MPICH was used instead.

## 2. What was done (workflow)

The original sources (`code_original/`, copied from `../../code/`, which is
read-only for the agent) were used as-is, then:

1. compiled with `-Wall -Wextra -pedantic -std=gnu11`;
2. compiled with `-fsanitize=address,undefined -g -O1` and run;
3. run under `timeout` with 2 and 4 ranks to detect deadlocks;
4. attached `gdb` to the still-running ranks to get full backtraces;
5. ran the fixed code under valgrind (`memcheck`, full leak check);
6. MUST was **not** available (it is only installed on LCC3 at
   `/scratch/c703429/software/must-1.9.1`, which does not exist on this machine),
   so its role is described below instead.

The fixed sources are in `code_fixed/`, the complete diff is in `fixes.patch`.

## 3. Findings — `example_1.c` (manual broadcast)

| # | Bug | Found with | Fix |
|---|-----|-----------|-----|
| 1 | Sender uses tag `42`, receiver waits for `tag2 == 43` — messages are never matched | code reading; `gdb` showed a rank blocked in `MPI_Recv` (line 37) | receiver uses `tag` |
| 2 | Rank 0 also sends to **itself** (`for i = 0 … nprocs`), then never receives; it blocks in `MPI_Barrier` while the other ranks block in `MPI_Recv` | code reading; `gdb` showed rank 0 at `MPI_Barrier` (line 40) | loop starts at `i = 1` |
| 3 | Unused variable `int i` (shadowed by the loop variables) | compiler warning `-Wunused-variable` | removed |
| 4 | `MPI_Send(&data, …)`: `&data` is a pointer-to-array (`int (*)[10]`), not the buffer; works only because the address is the same | code reading / `-Wall` does not flag it | `MPI_Send(data, …)` |
| 5 | No guard for `nprocs < 2` (the program is meaningless/quietly "succeeds" with one rank) | code reading | exit with an error message |

Runtime evidence (original code):

```
$ timeout 10 mpiexec -n 2 ./code_original/example_1_orig
[…]
[exit:124]          # 124 = killed by timeout → deadlock
[…]
# pid 39: main (…) at example_1.c:40   <- MPI_Barrier
# pid 40: main (…) at example_1.c:37   <- MPI_Recv
```

## 4. Findings — `example_2.c` (ring communication)

| # | Bug | Found with | Fix |
|---|-----|-----------|-----|
| 1 | `(rank = 1 + size) % size`: assignment inside the argument list **overwrites the local rank**; `rank` is read (`rank + 1`) and written in the same expression → undefined behaviour / unspecified evaluation order | compiler warning `-Wsequence-point`; code reading | removed the assignment; introduced `dest` / `source` |
| 2 | The "source" is computed as `(1 + size) % size`, a constant (`1` for `size == 2`), not the left neighbour. Both `MPI_Sendrecv` and the `MPI_Send`/`MPI_Recv` pair therefore talk to the wrong/mismatched ranks and hang | code reading; `gdb` showed one rank in `MPI_Sendrecv` (line 31) and the other in `MPI_Recv` (line 36) | `source = (rank - 1 + size) % size`, `dest = (rank + 1) % size` |
| 3 | Datatype mismatch: send is `1 × newType` (a contiguous 2-int datatype, 8 bytes) but receive is `sizeof(int) * 2 × MPI_BYTE`. The MPI type signatures do not match (the byte count happens to be equal, which is why MPICH accepts it at runtime) — this is an erroneous program that MUST would report | code reading, MPI standard type-signature rule; MPICH does not check it, MUST on LCC3 does | receive `1 × newType` on both paths |
| 4 | Blocking `MPI_Send` followed by `MPI_Recv` in a ring can deadlock once the message size exceeds the eager protocol threshold | code reading (classic ring pattern) | post `MPI_Irecv`, then `MPI_Isend`, then `MPI_Waitall` — still "Send and Recv", but deadlock-free |
| 5 | The committed datatype `newType` is never freed | code reading | `MPI_Type_free(&newType)` before `MPI_Finalize` |
| 6 | The received data is never checked, so wrong data would go unnoticed | code reading | check `rBuf` after both communications (and print an error otherwise) |
| 7 | `rank` was printed **after** it had been overwritten, so every rank prints "Signing off, rank 1" | code reading (consequence of bug 1) | fixed by not overwriting `rank` |

Runtime evidence (original code):

```
$ timeout 10 mpiexec -n 2 ./code_original/example_2_orig
Hello, I am rank 0 of 2 processes.
Hello, I am rank 1 of 2 processes.
[…]
[exit:124]

# pid 9:  main (…) at example_2.c:36   <- MPI_Recv        (second phase)
# pid 10: main (…) at example_2.c:31   <- MPI_Sendrecv    (first phase)
```

## 5. What the individual tools found (and did not find)

| Tool | Settings | Result |
|------|----------|--------|
| Compiler warnings | `-Wall -Wextra -pedantic -std=gnu11` | 2 real findings: unused `i` (`example_1`), sequence-point UB from `rank = …` (`example_2`). It did **not** catch the tag mismatch, the self-send, the datatype mismatch, the wrong neighbour or the leaks. |
| AddressSanitizer + UndefinedBehaviorSanitizer | `-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1`, `ASAN_OPTIONS=detect_leaks=0` | No memory errors, no UB reports in either program. Both still hung — sanitizers cannot see MPI-level deadlocks or type mismatches. |
| Runtime + timeout | `mpiexec -n 2` / `-n 4`, `timeout 10` | Both programs hang with 2 and 4 ranks (exit 124). Quickest way to prove something is wrong, but not *what*. |
| gdb | attach to each rank, `bt` | Pinpointed the exact blocking calls and source lines: `example_1.c:37` (`MPI_Recv`) vs `:40` (`MPI_Barrier`); `example_2.c:31` (`MPI_Sendrecv`) vs `:36` (`MPI_Recv`). This is what confirmed the "wrong neighbour / wrong tag" hypotheses. |
| Valgrind (memcheck) | `--leak-check=full --errors-for-leak-kinds=definite` on the fixed code | No invalid reads/writes, no uninitialised values in the user code. The only report is 640 bytes "definitely lost" inside `libfabric` allocated from `PMPI_Init` — an MPI/OFI-internal leak, not a bug in the example. |
| MUST | — | Not usable here (LCC3-only installation, `/scratch/c703429` absent). On LCC3 it would report the deadlock (as in exercise 1) and the mismatched send/recv type signatures in `example_2` (bug 3), which no local tool detected. |

## 6. Fixes applied

Full diff: `fixes.patch` (against `code_original/`). Summary:

**`example_1.c`**

```c
- if(myrank == 0) {
-     for(int i = 0; i < nprocs; i++)
-         MPI_Send(&data, count, MPI_INT, i, tag, MPI_COMM_WORLD);
- } else {
-     MPI_Recv(&data, count, MPI_INT, 0, tag2, MPI_COMM_WORLD, &status);
- }
+ if(myrank == 0) {
+     for(int i = 1; i < nprocs; i++)
+         MPI_Send(data, count, MPI_INT, i, tag, MPI_COMM_WORLD);
+ } else {
+     MPI_Recv(data, count, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);
+ }
```

plus the `nprocs < 2` guard, the removed unused `i`, and a Makefile with
`-Wall -Wextra -pedantic`.

**`example_2.c`**

```c
+ const int dest   = (rank + 1) % size;
+ const int source = (rank - 1 + size) % size;

- MPI_Sendrecv(sBuf, 1, newType, (rank + 1) % size, 123, rBuf, sizeof(int) * 2, MPI_BYTE,
-              (rank = 1 + size) % size, 123, MPI_COMM_WORLD, &status);
+ MPI_Sendrecv(sBuf, 1, newType, dest, 123, rBuf, 1, newType, source, 123,
+              MPI_COMM_WORLD, &status);

- MPI_Send(sBuf, 1, newType, (rank + 1) % size, 456, MPI_COMM_WORLD);
- MPI_Recv(rBuf, sizeof(int) * 2, MPI_BYTE, (rank = 1 + size) % size, 456, …);
+ MPI_Irecv(rBuf, 1, newType, source, 456, MPI_COMM_WORLD, &reqs[0]);
+ MPI_Isend(sBuf, 1, newType, dest,   456, MPI_COMM_WORLD, &reqs[1]);
+ MPI_Waitall(2, reqs, MPI_STATUSES_IGNORE);
```

plus the received-data check and `MPI_Type_free(&newType)`.

## 7. Verification after the fix

```
$ make -C code_fixed
# no warnings at all with -Wall -Wextra -pedantic

$ for p in 2 3 4 5; do
    mpiexec -n $p ./code_fixed/example_1   # prints "Rank N received correct data" for every N
    mpiexec -n $p ./code_fixed/example_2   # prints hello / signing off, exit 0
  done
# all runs finish with exit 0; no timeouts

$ mpiexec -n 1 ./code_fixed/example_1      # "This test needs at least 2 processes!" (exit 1)
$ mpiexec -n 1 ./code_fixed/example_2      # same

# ASan/UBSan build, 4 ranks: exit 0, 0 sanitizer reports
# valgrind memcheck, 2 ranks: no user-code errors (only the known libfabric leak)
```

## 8. Was the AI useful — and where are its limits?

* **Compiler warnings** and **sanitizers** alone would not have explained the
  hangs; they are necessary but not sufficient for MPI debugging.
* The decisive steps were **running with a timeout** (proves a deadlock) and
  **attaching gdb** to the blocked ranks (proves where).
* The **type-signature mismatch** and the **missing `MPI_Type_free`** are
  invisible to both sanitizers and MPICH; they were found by reading the code and
  the MPI standard and are exactly what MUST is for.
* The AI's proposed fix for `example_2` was to stop overwriting `rank` and to
  compute the left neighbour — this also incidentally fixed the "Signing off,
  rank 1" output and the wrong print.
* Limitations observed: without a real MPI installation nothing can be run; the
  sandbox blocked `sudo` and writing into `../../code/`, so the fixed code lives
  in `code_fixed/`; MUST is not installed locally, so its deadlock/type-mismatch
  reports are the one part of the exercise that could not be reproduced here.
