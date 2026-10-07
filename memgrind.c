/*
 * memgrind.c -- stress the allocator. A five-task workload runs 50 times and
 * the average time per run is reported. Every task frees everything it
 * allocates, so the bank should be back to a single free chunk between runs;
 * any leak or error message means the allocator mishandled the traffic.
 *
 * Tasks 1-3 are the ones named in the writeup; tasks 4-5 are ours:
 *   4. free adjacent pairs, then refill the holes with larger objects, and
 *      check each refill landed where its pair was freed;
 *   5. push/pop a stack of nodes whose payloads hold pointers and counters,
 *      so client data that looks like addresses survives untouched.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "mymalloc.h"

#define RUNS  50
#define SMALL 120 /* simultaneous small objects in tasks 2 and 3 */

/* The workload is correct code: a failed allocation is a bug in the
 * allocator or a bank that is too small, so stop and say so. */
static void *checked(size_t size)
{
    void *p = malloc(size);

    if (p == NULL) {
        fprintf(stderr, "memgrind: workload out of memory\n");
        exit(EXIT_FAILURE);
    }
    return p;
}

static double elapsed(struct timeval start, struct timeval stop)
{
    return (double)(stop.tv_sec - start.tv_sec)
           + (double)(stop.tv_usec - start.tv_usec) / 1000000.0;
}

/* Task 1: one object of each size, freed in reverse order. */
static void task_sizes(void)
{
    static const size_t sizes[] = {8, 16, 32, 64, 128, 512, 1024};
    const int n = (int)(sizeof(sizes) / sizeof(sizes[0]));
    void *p[sizeof(sizes) / sizeof(sizes[0])];

    for (int i = 0; i < n; i++)
        p[i] = checked(sizes[i]);
    for (int i = n - 1; i >= 0; i--)
        free(p[i]);
}

/* Task 2: SMALL small objects, freed in the order they were allocated. */
static void task_sequential(void)
{
    void *p[SMALL];

    for (int i = 0; i < SMALL; i++)
        p[i] = checked(8);
    for (int i = 0; i < SMALL; i++)
        free(p[i]);
}

/* Task 3: repeatedly choose between allocating a 1-byte object and freeing a
 * random live object; once SMALL allocations have happened, free the rest. */
static void task_random(void)
{
    void *p[SMALL];
    int live = 0;
    int made = 0;

    while (made < SMALL) {
        if (live == 0 || rand() % 2 == 1) {
            p[live++] = checked(1);
            made++;
        } else {
            int i = rand() % live;
            free(p[i]);
            p[i] = p[live - 1]; /* the hole in the array takes the last slot */
            live--;
        }
    }
    for (int i = 0; i < live; i++)
        free(p[i]);
}

/* Task 4: allocate objects in pairs, free each pair, then allocate objects
 * twice as large. A 40-byte payload needs a 48-byte chunk, exactly two merged
 * 24-byte chunks, and first fit must hand it back at the pair's own address. */
static void task_pairs(void)
{
    enum { PAIRS = 24 };
    void *p[2 * PAIRS];
    void *hole[PAIRS]; /* the address each pair started at */

    /* All 48 objects are live at once, so they sit back to back: pair i
     * starts 48 bytes after pair i - 1. */
    for (int i = 0; i < 2 * PAIRS; i++)
        p[i] = checked(16);
    for (int i = 0; i < PAIRS; i++)
        hole[i] = p[2 * i];
    for (int i = 0; i < 2 * PAIRS; i += 2) {
        free(p[i]);
        free(p[i + 1]);
    }

    /* A 40-byte payload needs a 48-byte chunk, exactly one merged pair.
     * With the pairs merged, first fit hands these back at the addresses the
     * pairs occupied, in order; unmerged 24-byte chunks cannot hold the
     * payload, so first fit would reach the free tail past them instead. */
    for (int i = 0; i < PAIRS; i++) {
        p[i] = checked(40);
        if (p[i] != hole[i]) {
            fprintf(stderr,
                    "memgrind: task 4 refill at %p, expected the coalesced pair at %p\n",
                    p[i], hole[i]);
            exit(EXIT_FAILURE);
        }
    }
    for (int i = 0; i < PAIRS; i++)
        free(p[i]);
}

/* Task 5: a stack of nodes, pushed and popped in uneven rounds. Each payload
 * holds a pointer and a counter, which the allocator must treat as opaque
 * data: it never inspects a payload to find the next chunk. */
static void task_stack(void)
{
    struct node {
        void *next;
        unsigned long tag;
    };
    enum { DEPTH = 48, ROUNDS = 3 };

    struct node *top = NULL;

    for (int round = 0; round < ROUNDS; round++) {
        for (int i = 0; i < DEPTH; i++) {
            struct node *n = checked(sizeof(struct node));
            n->next = top;
            n->tag = round * DEPTH + i;
            top = n;
        }
        for (int i = 0; i < DEPTH / 2; i++) {
            struct node *n = top;
            top = top->next;
            free(n);
        }
    }
    while (top != NULL) {
        struct node *n = top;
        top = top->next;
        free(n);
    }
}

int main(void)
{
    struct timeval start, stop;

    srand(20261007); /* a fixed seed keeps the runs comparable */

    gettimeofday(&start, NULL);
    for (int run = 0; run < RUNS; run++) {
        task_sizes();
        task_sequential();
        task_random();
        task_pairs();
        task_stack();
    }
    gettimeofday(&stop, NULL);

    double total = elapsed(start, stop);
    printf("%d runs, total %.6f s, average %.6f s per run\n",
           RUNS, total, total / RUNS);

    return EXIT_SUCCESS;
}
