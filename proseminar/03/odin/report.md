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

After that I ran it again with must. Must now says rank 0 still has a potential deadlock. This is because it also sends to itself, which could potentially be blocking leading to an imeadiate deadlock. The fix is simply to remove the send to rank 0.

### example_2
Here the gdb tells me, that it one process is stuck at the sendrecv and the other at the second receive.
This is obviously because the sending pattern doesn't make any sense at all lol.
The destination is correct but the source is like so weird. I fixed it by making the source `int source = (rank - 1 + size) % size`, which is the left neighboor with a wraparound to size-1 for rank = 0;
The same problem obviously applies to the manual send and receive. Where i just added the same source and destination.

After that i ran it again with must. There are still more errors. Namely Type Mismatch twice, a deadlock and an error leak datatype.

The type mismatch is easy, since we should just match the send and recieve. The error leak datatype is alsoo not difficult, we simply need to free the new datatype with `MPI_Type_free(&newType)`.  

The deadlock can only happend if the send is blocking. In order to fix this, I just made the calls non-blocking.



No tool gave the warning that `&data` instead of just `data` got passed as the buffer in example_1. This is probably because it won't lead to undefined behaviour/is not really a bug, I think.


## Sanitizer

I ran it once with `-fsanitize=undefined` `-fsanitize=address`

There is no undefined behaviour the sanitizer could find, but the address lead to the program crashing. After further Investigation with gdb, I found that the MPI_Init was the reason for both programs, so I assume it doesn't work properly with MPI (IDK lol)

# Exercise 2

For this exercise I tried this with Opus 5.5, Sonnet 5.5 and Haiku 5.5

I gave them all the same prompt:

"Hello, for this task only focus on the code_fixed_(model) folder. In it, there are 2 mpi programs which are error prone. Try to fix them using any tools available. After everything, give me a report on what you did"

## Results

## Opus

Opus had pretty similar fixes to me. It also came to the same conclusion regarding the address sanitizer. And added some additional checks and timings. Here is the report written by Opus:

> I fixed both programs in `code_fixed_ai/`. Both now finish correctly with 2, 3, 4, 7 and 16 processes, and every receiving rank gets the right data.
>
> ### How I found the bugs
> 1. **Compiler warnings** (`-Wall -Wextra -pedantic`): an unused `int i` in example_1, and `-Wsequence-point` on `rank` in example_2.
> 2. **Running with a timeout**: both original programs hang (exit code 124).
> 3. **gdb attached to the hung ranks**:
>    - example_1: rank 0 waits in `MPI_Barrier` (line 40) while every other rank is stuck in `MPI_Recv` (line 37).
>    - example_2: ranks are stuck in `MPI_Sendrecv` (line 31) and `MPI_Recv` (line 36).
> 4. **clang-tidy MPI checks** found nothing useful. They can't see wrong tags or derived datatypes, so the rest came from reading the code.
> 5. **Forcing synchronous sends**: I added a small header (`#define MPI_Send MPI_Ssend`) so every send has to wait for its matching receive. This exposes sends that only work because Open MPI happens to buffer small messages.
>
> ### example_1.c (a hand-written broadcast)
>
> | Bug | Fix |
> |---|---|
> | Rank 0 sends with `tag` but the others receive with `tag2`, so nothing matches and it deadlocks | Use `tag` on both sides and drop `tag2` |
> | Rank 0 also sends to itself (loop starts at `i = 0`) with no matching receive. It only works if the message gets buffered | Start the loop at `i = 1` |
> | `&data` (pointer to the whole array) passed as the buffer | Pass `data` |
> | Unused outer `int i`; `const int count` makes `data[count]` a variable-length array | Removed `i`; `#define COUNT 10` |
> | The header comment says the operation is timed, but it never was | Added `MPI_Wtime` around the broadcast, with barriers before and after; rank 0 prints the time |
>
> ### example_2.c (two ring exchanges)
>
> | Bug | Fix |
> |---|---|
> | `(rank = 1 + size) % size` is a typo for `-`. It **overwrites `rank`** and isn't even a valid neighbour. With `rank` used twice in one call, the order is also undefined (the compiler warning) | `int dest = (rank + 1) % size; int source = (rank - 1 + size) % size;` |
> | The send uses 1 `newType` (2 ints) but the receive uses 8 `MPI_BYTE`. That type mismatch is not allowed by the MPI standard | Receive with `1, newType` |
> | Step 3: every rank calls blocking `MPI_Send` before `MPI_Recv`. That's a cyclic wait that only works while MPI buffers the message | `MPI_Isend` → `MPI_Recv` → `MPI_Wait` |
> | `newType` was never freed | `MPI_Type_free(&newType)` |
> | Received data was never checked | Reset `rBuf` before each exchange and check it equals `{1, 2}` afterwards |
>
> ### Verification
> - **Warnings:** none with `-Wall -Wextra -pedantic -Wshadow`. The Makefile still builds both programs.
> - **Runs:** exit code 0 at every process count above, with correct output on every rank.
> - **Synchronous sends:** both fixed programs still pass with 4 and 7 processes. I also put the blocking `MPI_Send` back into fixed example_2: it hangs, which shows the `MPI_Isend` change is needed.
> - **UBSan:** no reports.
> - **Valgrind:** its only reports are inside PMIx, Open MPI's process-launch library, not in our code.
> - **ASan:** both programs crash with a segfault at startup, but so does an empty `MPI_Init`/`MPI_Finalize` program. That's an incompatibility between ASan and this Open MPI build, so ASan gave no useful result.
>


## Sonnet

The changes were almost identical to mine. Comparing to opus, sonnet also did not add more checks or timings. Opus also used more tools.

Here is the report written by Sonnet:

> I fixed both programs in `code_fixed_sonnet/`. They now compile without warnings and finish with exit code 0 on 2, 3 and 7 processes.
>
> ### How I found the bugs
> 1. **Compiler warnings** (`-Wall -Wextra -pedantic`): an unused `int i` in example_1 and `-Wsequence-point` on `rank` in example_2.
> 2. **Running with a timeout**: both originals hang on 4 processes (exit code 124).
> 3. **Reading the code** to find the causes of the hangs. I did not use MUST, gdb or the sanitizers.
>
> ### example_1.c (hand-written broadcast)
>
> | Bug | Fix |
> |---|---|
> | Rank 0 sends with `tag` but the other ranks receive with `tag2`. Nothing matches, so the receivers block forever and rank 0 sits in the barrier | Receive with `tag`, remove `tag2` |
> | Rank 0 also sends to itself (loop starts at `i = 0`) and never receives. This only works if MPI buffers the message | Start the loop at `i = 1` |
> | `&data` (pointer to the whole array) passed as the buffer | Pass `data` |
> | Unused outer `int i` | Removed |
>
> ### example_2.c (two ring exchanges)
>
> | Bug | Fix |
> |---|---|
> | `(rank = 1 + size) % size` is an assignment, probably a typo for `-`. It overwrites `rank` and gives the wrong source. Using `rank` twice in one call also makes the evaluation order undefined | `dest = (rank + 1) % size` and `source = (rank - 1 + size) % size`, computed once |
> | The send uses `1 x newType` but the receive uses `8 x MPI_BYTE`. The MPI standard does not allow this type mismatch | Receive with `1, newType` |
> | In step 3 every rank calls blocking `MPI_Send` before `MPI_Recv`. That is a cyclic wait and only works if MPI buffers the message | `MPI_Isend`, then `MPI_Recv`, then `MPI_Wait` |
> | `newType` was never freed | Added `MPI_Type_free(&newType)` |
>
> ### Verification
> - **Warnings:** none with `-Wall -Wextra -pedantic`.
> - **Runs:** both programs exit 0 on 2, 3 and 7 processes.
> - **Data check:** example_1 never printed "Whoa! The data is incorrect". Example_2 prints no received data, so its payload was not checked.
> - **Makefile:** `make` builds both programs. I left it unchanged.


## Haiku

Haiku also had a similar outcome but mainly just analyzed the code and looked at compiler warnings.

Here is the report generated by Haiku.

I fixed both programs in `code_fixed_haiku/`. Both hung under `mpirun` before the fix and now finish with exit code 0 at every process count I tested (1, 2, 3, 5 and 8).

### How I found the bugs
1. **Reading the code** for the mismatched tags, the self-send and the corrupted `rank`.
2. **Running the originals with a timeout**: both hang on 4 processes (exit code 124).
3. **Compiling the fixes** with `-Wall -Wextra`.

### example_1.c (hand-written broadcast)

| Bug | Fix |
|---|---|
| Rank 0 sends with `tag` but the other ranks receive with `tag2`. Nothing matches, so the receivers block forever | Use `tag` on both sides, remove `tag2` |
| Rank 0 also sends to itself (loop starts at `i = 0`) and never receives | Start the loop at `i = 1` |
| `&data` passed as the buffer | Pass `data` |
| Unused outer `int i` | Removed |

### example_2.c (datatypes, Sendrecv and a ring with Send/Recv)

| Bug | Fix |
|---|---|
| `(rank = 1 + size) % size` is an assignment, not a comparison. It overwrites `rank` with `size + 1`, so every later neighbour is wrong | `int left = (rank - 1 + size) % size;` |
| The send uses `1 x newType` but the receive uses `8 x MPI_BYTE`, a type-signature mismatch | Receive with `1, newType` |
| In step 3 every rank calls blocking `MPI_Send` before `MPI_Recv`, which can deadlock once messages exceed the eager limit | Rank 0 receives first, the others send first; still plain `MPI_Send`/`MPI_Recv` |
| `newType` was never freed | Added `MPI_Type_free(&newType)` |
| `//) Create` typo | `// 1) Create` |

### Verification
- **Compiler:** both files compile with no warnings under `-Wall -Wextra`.
- **Runs:** both programs exit 0 on 1, 2, 3, 5 and 8 processes. example_1 prints "received correct data" on every non-root rank.
- **Limits:** example_2 prints no received values, so I checked that it completes and that the ranks are correct, not the payload. I did not use a debugger or sanitizer.
- **Makefile:** unchanged. I built into the scratchpad so no binaries were added to the folder.
- **Changes are uncommitted.** The folder is untracked in git.
