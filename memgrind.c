#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "mymalloc.h"

#define RUNS  50
#define SMALL 120

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

static void task_sequential(void)
{
    void *p[SMALL];

    for (int i = 0; i < SMALL; i++)
        p[i] = checked(8);
    for (int i = 0; i < SMALL; i++)
        free(p[i]);
}

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
            p[i] = p[live - 1];
            live--;
        }
    }
    for (int i = 0; i < live; i++)
        free(p[i]);
}

static void task_pairs(void)
{
    enum { PAIRS = 24 };
    void *a[PAIRS];
    void *b[PAIRS];
    void *spacer[PAIRS];
    void *hole[PAIRS];

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

    /* Live spacers isolate each 48-byte hole; a 40-byte request only fits
     * there if its two 24-byte chunks coalesced. */
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

    srand(20261007);

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
