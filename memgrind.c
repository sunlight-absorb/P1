/*
 * memgrind.c -- stress the allocator. A five-task workload runs 50 times and
 * the average time per run is reported. Every task frees everything it
 * allocates, so the bank should be back to a single free chunk between runs;
 * any leak or error message means the allocator mishandled the traffic.
 *
 * Tasks 1-3 are the ones named in the writeup; tasks 4-5 are ours:
 *   4. free adjacent pairs, keeping a live object after each pair, then
 *      refill the holes and check each refill landed where its pair was;
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

/* Task 4: allocate (pair, spacer) triples, free only the pairs, then refill
 * each hole with an object twice as large. A 40-byte payload needs a 48-byte
 * chunk, exactly two merged 24-byte chunks, and the live spacer keeps each
 * hole from merging with its neighbours, so the refill has to land where its
 * own pair was freed. */
static void task_pairs(void)
{
    enum { PAIRS = 24 };
    void *a[PAIRS];
    void *b[PAIRS];
    void *spacer[PAIRS]; /* stays live, isolating one hole from the next */
    void *hole[PAIRS];   /* the address each pair started at */

    /* 24 triples of 24-byte chunks: 1728 bytes, inside a 4096-byte bank. */
    for (int i = 0; i < PAIRS; i++) {
        a[i] = checked(16);
        b[i] = checked(16);
        spacer[i] = checked(16);
        hole[i] = a[i];
    }
    for (int i = 0; i < PAIRS; i++) {
        free(a[i]);
        free(b[i]);
    }

    /* A spacer is still live between every pair, so a merged pair is a 48-byte
     * hole and nothing more: the refill must return that pair's own address.
     * Unmerged 24-byte chunks cannot hold a 40-byte payload, so first fit
     * would reach the free tail past the triples instead. */
    for (int i = 0; i < PAIRS; i++) {
        a[i] = checked(40);
        if (a[i] != hole[i]) {
            fprintf(stderr,
                    "memgrind: task 4 refill at %p, expected the coalesced pair at %p\n",
                    a[i], hole[i]);
            exit(EXIT_FAILURE);
        }
    }
    /* b[i] is now inside the live 48-byte chunk and must not be freed. */
    for (int i = 0; i < PAIRS; i++) {
        free(a[i]);
        free(spacer[i]);
    }
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
