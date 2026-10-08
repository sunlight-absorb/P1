Prashanth Babu pb608



Testing strategy

Run make check. This command builds all programs and runs all tests.
If a test fails, make check stops at that test. If all tests pass, the
last line is "check: all tests passed".

The tests are in two files. tests.c tests the correct use of malloc()
and free(). It covers requirements 1 to 3. test_errors.c runs one leak
scenario or error scenario for each run. The argument selects the
scenario. It covers requirements 4 and 5.

Each requirement below has three parts: the requirement, the method that
finds a violation, and the test that uses that method.

Requirement 1
malloc() reserves unallocated memory. When malloc() is successful, the
object that it returns does not overlap with other allocated objects.

The test_no_overlap test in tests.c allocates four 1000-byte objects.
It fills object 1 with the value 1, object 2 with the value 2, object 3
with 3, and object 4 with 4. Then it makes sure that each object still
contains its own value. If two objects overlap, the second fill changes
the bytes of the first object.

memtest.c comes with the assignment. It does the same test with 64
smaller objects. These objects fill the full heap. memtest must print
"0 incorrect bytes".

Requirement 2
free() deallocates memory. After the program frees an object, malloc()
can allocate that memory again.

The test_free_deallocates test in tests.c allocates a 3000-byte object
and frees it. It does this 10 times. The 4096-byte heap holds only one
3000-byte object. If free() does not release the memory, the second
malloc() call fails.

Requirement 3
free() coalesces adjacent free chunks. That is, free() merges two free
chunks that are next to each other into one chunk. To find a violation,
the program frees two adjacent objects. Then it requests a size that is
too large for each chunk alone. The request is successful only if free()
merged the two chunks.

The test_coalesce test in tests.c allocates four 1000-byte objects.
These objects almost fill the heap. The test frees the two middle
objects and requests 2000 bytes. This request is successful only if the
two free chunks merged.

Then the test frees all objects and requests 4088 bytes. This size is
the heap size minus one 8-byte header. This request is successful only
if all chunks merged into one chunk again.

Requirement 4
The allocator finds leaked objects and reports them. When the program
exits, the allocator prints the number of objects that the program did
not free. It also prints the total size of these objects. If the
program freed all objects, the allocator prints nothing.

The command ./test_errors leak allocates 16, 8, and 32 bytes. It frees
the 8-byte object and then exits. The output must be
"mymalloc: 48 bytes leaked in 2 objects."

The command ./test_errors noleak allocates two objects, frees the two
objects, and exits. The program must print nothing.

Requirement 5
free() finds pointers that it cannot free. In that case, free() prints
"free: Inappropriate pointer" with the file name and the line number.
Then free() stops the program with exit status 2.

Three commands test this requirement. Each command must exit with
status 2. ./test_errors outside frees the address of a local variable.
./test_errors interior frees a pointer to the middle of an object.
./test_errors double frees the same object two times.

The interior scenario stops before it can free its object. Thus, its
leak report "16 bytes leaked in 1 objects" is correct.

Shared heap
tests.c runs all its tests in one process. Thus, all tests use the same
heap. The free() test runs first, when the heap is new. If one test
fails, the tests after it can also fail. In that case, start with the
first FAIL line.

make check runs memgrind last. memgrind is the performance program. It
is not a test.




Performance program

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
