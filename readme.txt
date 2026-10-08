Prashanth Babu pb608



Test plan

Run make check to build and run all test programs.

The tests cover separate allocations, reuse after free(), alignment, and
merges of adjacent free chunks. They also cover invalid free() calls,
allocation failure, and memory leaks. A memory leak is an allocation that
the program does not free before exit.

For correct allocator use, the programs must preserve data and free all
allocations. For invalid free() calls, each child process must print an
error and exit with status 2. A child process is a separate process that
fork() creates. This lets test_errors continue after an invalid free()
ends the child process.

Make sure that memtest and memtest-real each print "0 incorrect bytes".
Make sure that the leak reports match the expected byte and object counts.

The leak tests print expected reports to stderr, the standard error stream.
The oversized allocation tests also print expected errors to stderr.
Some child processes in test_errors exit with allocations that they did
not free, so their leak reports are expected.

The final message from make check is "check: all test programs passed".
The command uses exit status to detect failures. It does not compare the
leak reports or the byte count from memtest against expected output.




Test programs

None of the test programs use command-line arguments.

The names after make are Makefile targets, not arguments to the test
programs. A target tells make which program to build or which action to
run. A build command does not run the program unless its target also
includes that action.

make builds the main test programs. It does not build memtest-real or
memtest-leak.

make memtest-real builds the version that uses the standard C allocator.
The command ./memtest-real runs that program without arguments.

make memtest-leak builds the version that deliberately leaves allocations
without a free() call. The command ./memtest-leak runs that program
without arguments.

make check builds and runs all test programs, including both versions.
The name check is a Makefile target, not a separate test program.

For the other test programs, the build target matches the program name.
For example, make memtest builds memtest. The command ./memtest runs it
without arguments.

memtest
This program allocates 64 objects with 56 bytes each. With the 8-byte
headers, these objects fill the default 4096-byte heap. The program writes
a different byte value into each object. It then compares every byte with
the expected value and frees all objects. Incorrect bytes can indicate
that allocations overlap or that the allocator changed program data.

memtest-real
This version of memtest uses the standard C allocator instead of mymalloc.
It provides a comparison for the byte-pattern test. The Makefile builds
it from memtest.c with -DREALMALLOC.

memtest-leak
This version of memtest does not free its 64 objects. The expected exit
report is "mymalloc: 3584 bytes leaked in 64 objects." The Makefile builds
it from memtest.c with -DLEAK.

test_free_reuse
This program allocates three 128-byte objects and fills each with a
different byte value. It frees the middle object and requests another
128-byte object. The new object must use the same address as the freed
object. The test also makes sure that writes to the new object do not
change either neighboring object.

test_coalesce
Coalescing means that the allocator merges adjacent free chunks into one
larger free chunk. This program frees three adjacent objects between two
objects that remain allocated. It requests 192 bytes, which fit in the
merged space but not in any one of the original chunks. The returned
pointer must match the start of that space. After all objects are free,
a request for 4088 bytes must use the whole default heap except its header.

test_alignment
This program requests 0, 1, 7, 8, 9, 20, 24, and 100 bytes. Each returned
address must be a multiple of 8. The distance between consecutive
allocations must equal the rounded payload size plus the 8-byte header.
The zero-byte request must reserve an 8-byte payload. A request for more
than the heap size must return NULL.

test_errors
This program runs each invalid free() call in a child process. The cases
include a pointer to a local variable, a pointer inside an allocation,
a second free() of the same allocation, and free(NULL). Each child must
exit with status 2. A separate case makes sure that a request for more
than the heap size returns NULL. If all cases pass, the program reports
five successful checks.

test_leak
This program requests 10, 20, and 30 bytes, then frees only the middle
object. The allocator rounds the remaining payloads to 16 and 32 bytes.
The expected exit report is "mymalloc: 48 bytes leaked in 2 objects."

memgrind
This program runs five tasks in sequence, then repeats that sequence
50 times. It reports the total time and the average time per sequence.
It uses a fixed seed for rand() so that the random sequence is repeatable.

The first task allocates objects of 8, 16, 32, 64, 128, 512, and 1024 bytes.
It frees them in reverse order. The second task allocates 120 objects of
8 bytes each, then frees them in allocation order. The third task mixes
allocation and free() calls until it allocates 120 one-byte objects.
It then frees all remaining objects.

The fourth task allocates 24 groups of three 16-byte objects. It frees
the first two objects in each group. The third object keeps each free
pair separate from the next pair. Each new 40-byte allocation must reuse
the start of a merged pair. Without the merge, neither original chunk
can hold the new allocation.

The fifth task uses a linked stack of nodes that contain a pointer and
a number. Each of three rounds adds 48 nodes and frees the top 24 nodes.
The task then frees the remaining nodes. This task exercises repeated
allocation and free() calls with pointers stored in program data.




Design notes

The heap is the fixed memory area that this allocator manages. It uses
one static byte array with a default size of 4096 bytes. A union with a
double provides alignment for the array. MEMLENGTH sets the array size
at compile time. The allocator uses only complete multiples of 8 bytes.

Each chunk contains an 8-byte header followed by a payload. The payload
is the memory that the caller can use. The header contains two unsigned
int fields: the total chunk size and a flag for allocation state.
The allocator finds the next chunk by adding the current chunk size to
its address. It does not store pointers between chunks.

mymalloc() rounds the requested payload size up to a multiple of 8.
malloc(0) reserves an 8-byte payload. The allocator uses the first free
chunk large enough for the request. If at least 16 bytes remain, the
allocator creates a separate free chunk from the remainder. A smaller
remainder stays in the allocated chunk.

myfree() makes sure that the pointer is the start of an allocated payload.
It marks that chunk free and merges it with adjacent free chunks.
This makes the combined space available for larger requests.

The first malloc() or non-NULL free() call initializes the heap as one
free chunk. A separate initialized flag records this state. Initialization
also registers the leak report function with atexit(). The report counts
payload bytes and padding, but not headers.

The macros in mymalloc.h pass the source file name and line number to
mymalloc() and myfree(). If an allocation fails, mymalloc() prints the
requested size and source location, then returns NULL.

An invalid free() prints the source location, pointer value, and reason,
then calls exit(2). Invalid pointers include pointers outside the heap,
pointers inside a payload, and pointers to allocations that are already
free. free(NULL) also reports an error and exits with status 2.
