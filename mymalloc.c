/*
 * mymalloc.c -- a heap allocator that behaves like malloc()/free(), but
 * reports the misuse the real allocator would silently accept:
 *
 *   - a request that cannot be satisfied (NULL plus a message),
 *   - free() on a pointer that was never allocated, that points into the
 *     middle of a chunk, or that is freed a second time (message plus exit(2)),
 *   - objects still allocated at exit (leak report, registered with atexit()).
 *
 * The heap is a single static array; both the payloads handed to client code
 * and the metadata that tracks them live inside it.  The only storage outside
 * the heap is the initialization flag, which the assignment permits.
 *
 * Layout of the heap: a sequence of contiguous chunks, each an 8-byte header
 * followed by a payload.  A chunk's size covers header plus payload, so the
 * next chunk is found by adding that size to the chunk's address: the list is
 * threaded by size alone, with no explicit next pointer.  Every chunk, header,
 * and payload begins at an offset that is a multiple of 8, so the smallest
 * possible chunk is 16 bytes.
 */

#include <stdio.h>
#include <stdlib.h>

/* Figure 2: the union with a double is a portable way to force 8-byte
 * alignment of heap.bytes; heap.not_used is never read or written.
 * MEMLENGTH is a macro so test builds can use -DMEMLENGTH=... for larger or
 * smaller memory banks. */
#ifndef MEMLENGTH
#define MEMLENGTH 4096
#endif

/* All chunk offsets are multiples of ALIGNMENT. */
#define ALIGNMENT   8
#define HEADER_SIZE 8
#define MIN_CHUNK   (HEADER_SIZE + ALIGNMENT) /* smallest chunk: 16 bytes */
#define HEAP_BYTES  (MEMLENGTH - (MEMLENGTH % ALIGNMENT))

#if MEMLENGTH < MIN_CHUNK
#error "MEMLENGTH must be at least MIN_CHUNK bytes"
#endif

static union {
    char bytes[MEMLENGTH];
    double not_used;
} heap;

/* The one piece of storage the assignment allows outside the heap. */
static int initialized = 0;

/* Chunk metadata: exactly HEADER_SIZE bytes, stored in the heap itself.
 * An unsigned int holds any chunk size in a bank this small, and keeps the
 * header to 8 bytes; a size_t would push the minimum chunk to 24 bytes. */
typedef struct {
    unsigned int size;      /* chunk length, a multiple of ALIGNMENT */
    unsigned int allocated; /* 1 = in use, 0 = free */
} chunk_header;

/* ---------------------------------------------------------------- helpers */

/* The header of the chunk that begins at chunk. */
static chunk_header *header_of(void *chunk)
{
    return (chunk_header *)chunk;
}

/* The chunk after chunk, found from its size. */
static void *next_of(void *chunk)
{
    return (char *)chunk + header_of(chunk)->size;
}

/* The payload the client sees: the chunk minus its header. */
static void *payload_of(void *chunk)
{
    return (char *)chunk + HEADER_SIZE;
}

/* One past the last usable byte of the bank. */
static void *heap_end(void)
{
    return heap.bytes + HEAP_BYTES;
}

/* Report a bad free() and end the process, as the assignment requires.
 * The first line has the required form; the second names the specific
 * mistake and the offending pointer. */
static void report_bad_free(void *ptr, char *file, int line, const char *why)
{
    fprintf(stderr, "free: Inappropriate pointer (%s:%d)\n", file, line);
    fprintf(stderr, "free:   %p is %s\n", ptr, why);
    exit(2);
}

/* Walk the chunk list and report anything still allocated at exit.
 * Registered with atexit(); it must not call exit() itself. */
static void leak_check(void)
{
    size_t bytes = 0;
    int objects = 0;

    if (!initialized)
        return; /* the bank was never used, so there is nothing to walk */

    for (void *c = heap.bytes; c < heap_end(); c = next_of(c)) {
        chunk_header *h = header_of(c);
        if (h->size == 0)
            break; /* defensive: a zero-size chunk cannot advance the walk */
        if (h->allocated) {
            objects++;
            bytes += h->size - HEADER_SIZE; /* payload only, padding included */
        }
    }

    if (objects > 0)
        fprintf(stderr, "mymalloc: %zu bytes leaked in %d objects.\n", bytes, objects);
}

/* Lay out the initial free chunk covering the whole bank, and arrange for
 * the leak report to run at exit. */
static void init_heap(void)
{
    chunk_header *first = header_of(heap.bytes);

    if (sizeof(chunk_header) != HEADER_SIZE) {
        fprintf(stderr, "mymalloc: internal error: header is %zu bytes, expected %d\n",
                sizeof(chunk_header), HEADER_SIZE);
        exit(1);
    }

    first->size = HEAP_BYTES;
    first->allocated = 0;
    initialized = 1;
    atexit(leak_check);
}

/* Merge a free chunk with whichever neighbours are also free, so the bank is
 * always maximally coalesced and malloc() never has to search for space of
 * its own. Doing this in free() is cheaper and less error-prone than doing
 * it in malloc() when a request fails. */
static void coalesce(void *chunk)
{
    chunk_header *h = header_of(chunk);

    /* Absorb the following chunk, if it is free. */
    if ((char *)chunk + h->size < (char *) heap_end ()) {
        chunk_header *next = header_of((char *)chunk + h->size);
        if (!next->allocated)
            h->size += next->size;
    }

    /* Absorb the preceding chunk, if it is free. Chunks are only threaded
     * forward, so the predecessor is found by walking from the start. */
    for (void *c = heap.bytes; c < heap_end(); c = next_of(c)) {
        if (next_of(c) == chunk) {
            chunk_header *p = header_of(c);
            if (!p->allocated)
                p->size += h->size;
            break;
        }
    }
}

/* ------------------------------------------------------------------- API */

/* First-fit allocation: take the first free chunk that holds the request,
 * split off a remainder when what is left over could hold another chunk,
 * and return a pointer to the payload, not the header. */
void *mymalloc(size_t size, char *file, int line)
{
    if (!initialized)
        init_heap();

    if (size > HEAP_BYTES - HEADER_SIZE) {
        fprintf(stderr, "malloc: Unable to allocate %zu bytes (%s:%d)\n", size, file, line);
        return NULL;
    }

    /* Round the payload up to a multiple of 8; malloc(0) still needs a chunk. */
    size_t payload = (size + ALIGNMENT - 1) & ~(size_t)(ALIGNMENT - 1);
    if (payload == 0)
        payload = ALIGNMENT;
    size_t need = payload + HEADER_SIZE;

    for (void *c = heap.bytes; c < heap_end(); c = next_of(c)) {
        chunk_header *h = header_of(c);
        if (h->allocated || h->size < need)
            continue;

        if (h->size - need >= MIN_CHUNK) { /* leave a usable remainder free */
            chunk_header *rest = header_of((char *)c + need);
            rest->size = h->size - need;
            rest->allocated = 0;
            h->size = need;
        }
        h->allocated = 1;
        return payload_of(c);
    }

    fprintf(stderr, "malloc: Unable to allocate %zu bytes (%s:%d)\n", size, file, line);
    return NULL;
}

/* Mark a chunk free and coalesce it. A pointer that matches no chunk payload
 * is either foreign or interior, and a payload whose chunk is already free is
 * a double free; both, plus a pointer outside the bank, are the errors
 * section 2.1 requires. NULL is not one of them: standard C makes freeing it a
 * no-op, and a caller that frees the result of a failed allocation reaches it
 * routinely, so it must not stop the process. */
void myfree(void *ptr, char *file, int line)
{
    if (!initialized)
        init_heap(); /* an empty bank can only make this a bad free */

    char *base = heap.bytes;
    char *end = base + HEAP_BYTES;

    if (ptr == NULL)
        return;

    if ((char *)ptr < base || (char *)ptr >= end) {
        report_bad_free(ptr, file, line, "not obtained from malloc");
        return;
    }

    for (char *c = base; c < end; c = next_of(c)) {
        if (ptr == payload_of(c)) {
            chunk_header *h = header_of(c);
            if (!h->allocated) {
                report_bad_free(ptr, file, line, "already freed");
                return;
            }
            h->allocated = 0;
            coalesce(c);
            return;
        }
    }

    report_bad_free(ptr, file, line, "not the start of a chunk");
}
