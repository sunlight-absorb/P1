# P1: my little malloc()

CS 214 Fall 2026 Project I: an allocator that behaves like `malloc()`/`free()`
and reports the misuse the real allocator would silently accept.

Build with `make`, then run `./memgrind`. `make check` runs every test.

## Files

| File | Purpose |
| --- | --- |
| `mymalloc.h` | Required header, exactly as in fig. 1: `malloc()`/`free()` macros that pass `__FILE__` and `__LINE__` to the real functions. |
| `mymalloc.c` | The allocator: heap, chunk headers, first-fit allocation, splitting, coalescing, error reporting, leak detection. |
| `memgrind.c` | Stress workload: five tasks, 50 runs, average time reported. |
| `memtest.c` | Provided test: 64 objects that must tile the bank without overlap. |
| `test_coalesce.c` | Adjacent free chunks merge and the merged space is usable. |
| `test_free_reuse.c` | Freed space is offered again, without disturbing live neighbours. |
| `test_alignment.c` | Payloads are 8-byte aligned; chunk sizes are the request rounded to 8 plus an 8-byte header. |
| `test_errors.c` | Each misuse scenario from section 2.1 is reported and exits with status 2. |
| `test_leak.c` | Leaked objects are counted and sized in the exit report. |
| `AUTHOR` | NetIDs of the authors. |

## Design

The heap is one static array (`heap.bytes`, `MEMLENGTH` bytes, 8-byte aligned
through the union with a `double` in fig. 2). Everything the allocator needs
lives inside it: each chunk is an 8-byte header followed by a payload, and the
chunks tile the bank with no gaps.

The header is two `unsigned int`s: the total chunk length (header plus payload)
and an allocated flag. Keeping it to 8 bytes is what makes the 16-byte minimum
chunk possible, and it is why 120 one-byte objects fit in a 4096-byte bank.
A `size_t` size field would have pushed the header to 12 or 16 bytes.

Because a chunk's size is stored in its header, the next chunk is found by
adding that size to the chunk's address: the list is threaded by size, with no
explicit pointer. The first chunk always starts at the beginning of the bank.

Operations:

- `mymalloc()` rounds the request up to a multiple of 8 (a `malloc(0)` still
  gets an 8-byte payload), takes the first free chunk that fits, and splits
  off a remainder only when the remainder could hold another chunk.
- `myfree()` checks the pointer against the chunk list, marks the chunk free,
  then merges it with whichever neighbours are free. Coalescing in `free()`
  keeps the bank maximally merged, so `mymalloc()` never has to search for
  space of its own.
- Initialization is lazy: the first call to either function lays out the one
  free chunk covering the bank. The single `static int initialized` outside the
  heap is the only permitted exception. Initialization also registers the leak
  detector with `atexit()`, so no client has to call an init function and the
  code still compiles if the `#include "mymalloc.h"` line is removed.

Error reporting:

- An unsatisfiable request prints `malloc: Unable to allocate N bytes (file:line)`
  and returns `NULL`.
- A bad `free()` prints `free: Inappropriate pointer (file:line)`, then a line
  naming the specific mistake and the pointer value, and calls `exit(2)`.
  Detected: a pointer outside the bank (never from `malloc()`), a pointer
  that matches no chunk payload (interior), and a payload whose chunk is
  already free (double free) — the three scenarios section 2.1 requires.
  `free(NULL)` is a no-op, matching standard C: it is not one of the required
  errors, and a caller that frees the result of a failed allocation reaches it
  routinely, so stopping the process there would break correct client code.
- At exit, any still-allocated chunks are reported as
  `mymalloc: N bytes leaked in M objects.`, counting payload bytes only.

## Test plan

Each requirement below is checked by the named program; `make check` runs them
all. The tests are written as requirement → how a violation would show up →
program.

| Requirement | Violation would show up as | Test |
| --- | --- | --- |
| `malloc()` reserves unallocated memory | Filling each object with a distinct byte pattern and re-reading it finds overwritten bytes | `memtest` (64 objects that exactly tile the bank) |
| `free()` deallocates memory | A later request of the same size is refused, or reuses a live object, or a neighbour's pattern changes | `test_free_reuse` |
| Adjacent free chunks are coalesced | A request larger than any single chunk is refused after freeing neighbours, or the merged hole starts at the wrong address | `test_coalesce`, `task_pairs` in `memgrind` |
| Payloads are 8-byte aligned and chunk sizes are rounded | A returned pointer has low bits set, or the gap between consecutive chunks is not the request rounded to 8 plus 8 | `test_alignment` |
| Bad `free()` calls are reported and exit with status 2 | A child dies with a status other than 2, or returns at all | `test_errors` (forked scenarios: `free(&x)`, `free(p + 1)`, double free) |
| `free(NULL)` is a no-op | The child exits with 2 instead of returning | `test_errors` |
| An impossible request returns `NULL` with a message | The oversized request hands back a pointer | `test_alignment`, `test_errors` |
| Leaked objects are counted and sized | The exit report is missing, or names the wrong byte count or object count | `test_leak`, `memtest-leak` |
| The allocator survives heavy traffic | `memgrind` prints an out-of-memory or error message, or leaks | `memgrind` (5 tasks × 50 runs) |

`memtest-real` builds the same test against libc's allocator, as a baseline for
comparison.

## Notes

- `MEMLENGTH` is a macro: build with `-DMEMLENGTH=65536` to test a bigger bank.
- `memgrind` seeds `rand()` with a fixed value so the 50 runs are comparable.
- Tasks 4 and 5 of the `memgrind` workload are ours: freeing adjacent pairs and
  refilling the holes with double-sized objects, checking that each refill lands
  at the address its pair was freed (a 40-byte payload cannot fit an unmerged
  24-byte chunk, so broken coalescing would put it in the free tail instead),
  and pushing/popping a stack of nodes whose payloads hold pointers and counters
  (client data that looks like addresses must stay opaque to the allocator).
