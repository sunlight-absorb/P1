/* Chunks are contiguous; size includes the header and payload.
 * All chunk and payload offsets are multiples of 8. */

#include <stdio.h>
#include <stdlib.h>

/* Assignment bank: 4096 bytes by default; -DMEMLENGTH overrides test builds. */
#ifndef MEMLENGTH
#define MEMLENGTH 4096
#endif

#define ALIGNMENT   8
#define HEADER_SIZE 8
#define MIN_CHUNK   (HEADER_SIZE + ALIGNMENT)
#define HEAP_BYTES  (MEMLENGTH - (MEMLENGTH % ALIGNMENT))

#if MEMLENGTH < MIN_CHUNK
#error "MEMLENGTH must be at least MIN_CHUNK bytes"
#endif

/* Assignment fig. 2: the unused double provides alignment for heap.bytes. */
static union {
    char bytes[MEMLENGTH];
    double not_used;
} heap;

/* The one piece of storage the assignment allows outside the heap. */
static int initialized = 0;

/* unsigned int keeps the header at 8 bytes; size_t would enlarge it. */
typedef struct {
    unsigned int size;
    unsigned int allocated;
} chunk_header;

static chunk_header *header_of(void *chunk)
{
    return (chunk_header *)chunk;
}

static void *next_of(void *chunk)
{
    return (char *)chunk + header_of(chunk)->size;
}

static void *payload_of(void *chunk)
{
    return (char *)chunk + HEADER_SIZE;
}

static void *heap_end(void)
{
    return heap.bytes + HEAP_BYTES;
}

/* Section 2.1 requires this first diagnostic line and exit status 2. */
static void report_bad_free(void *ptr, char *file, int line, const char *why)
{
    fprintf(stderr, "free: Inappropriate pointer (%s:%d)\n", file, line);
    fprintf(stderr, "free:   %p is %s\n", ptr, why);
    exit(2);
}

static void leak_check(void)
{
    size_t bytes = 0;
    int objects = 0;

    if (!initialized)
        return;

    for (void *c = heap.bytes; c < heap_end(); c = next_of(c)) {
        chunk_header *h = header_of(c);
        if (h->size == 0)
            break;
        if (h->allocated) {
            objects++;
            bytes += h->size - HEADER_SIZE; /* payload only, padding included */
        }
    }

    if (objects > 0)
        fprintf(stderr, "mymalloc: %zu bytes leaked in %d objects.\n", bytes, objects);
}

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

/* No adjacent free chunks remain after free(), so one forward merge suffices. */
static void coalesce(void *chunk)
{
    chunk_header *h = header_of(chunk);

    if ((char *)chunk + h->size < (char *) heap_end ()) {
        chunk_header *next = header_of((char *)chunk + h->size);
        if (!next->allocated)
            h->size += next->size;
    }

    /* No back links: find the predecessor by walking from the start. */
    for (void *c = heap.bytes; c < heap_end(); c = next_of(c)) {
        if (next_of(c) == chunk) {
            chunk_header *p = header_of(c);
            if (!p->allocated)
                p->size += h->size;
            break;
        }
    }
}

void *mymalloc(size_t size, char *file, int line)
{
    if (!initialized)
        init_heap();

    /* Allocation failure must report the request and call site, then return NULL. */
    if (size > HEAP_BYTES - HEADER_SIZE) {
        fprintf(stderr, "malloc: Unable to allocate %zu bytes (%s:%d)\n", size, file, line);
        return NULL;
    }

    size_t payload = (size + ALIGNMENT - 1) & ~(size_t)(ALIGNMENT - 1);
    if (payload == 0)
        payload = ALIGNMENT;
    size_t need = payload + HEADER_SIZE;

    for (void *c = heap.bytes; c < heap_end(); c = next_of(c)) {
        chunk_header *h = header_of(c);
        if (h->allocated || h->size < need)
            continue;

        if (h->size - need >= MIN_CHUNK) {
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

void myfree(void *ptr, char *file, int line)
{
    if (ptr == NULL) {
        report_bad_free(ptr, file, line, "a null pointer");
        return;
    }

    if (!initialized)
        init_heap();

    char *base = heap.bytes;
    char *end = base + HEAP_BYTES;

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
