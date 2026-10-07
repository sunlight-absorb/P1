/*
 * Requirement: every payload is 8-byte aligned, and a chunk is the requested
 * size rounded up to a multiple of 8 plus an 8-byte header, so the smallest
 * chunk is 16 bytes.
 *
 * Detection: check the low bits of every pointer the library returns, and
 * measure the distance between consecutive chunk addresses while the bank is
 * otherwise empty. Also check the two boundary requests: malloc(0) must still
 * produce a usable chunk, and a request larger than the bank must return NULL
 * with an error message rather than handing out the whole bank.
 *
 * Test: allocate one object at a time from an empty bank and measure the gap
 * each request leaves behind.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "mymalloc.h"

#ifndef MEMLENGTH
#define MEMLENGTH 4096
#endif
#define HEADERSIZE 8

int
main (void)
{
    int failures = 0;

    /* Each request is alone in an empty bank, so the distance between its
     * payload and the next payload is exactly that chunk's length. */
    struct {
        size_t asked;
        size_t chunk; /* payload rounded to 8, plus an 8-byte header */
    } cases[] = {
        {0,   16}, {1,   16}, {7,   16}, {8,   16},
        {9,   24}, {20,  32}, {24,  32}, {100, 112},
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        char *p = malloc (cases[i].asked);
        if (p == NULL) {
            printf ("FAIL: request %zu returned NULL\n", cases[i].asked);
            failures++;
            continue;
        }
        if ((size_t) (uintptr_t) p % 8 != 0) {
            printf ("FAIL: payload for %zu bytes is misaligned: %p\n",
                    cases[i].asked, (void *) p);
            failures++;
        }

        char *q = malloc (1); /* the next chunk starts one chunk after p */
        if (q == NULL) {
            printf ("FAIL: follow-up allocation failed\n");
            failures++;
            break;
        }
        /* p and q are adjacent chunks, so the distance between their
         * payloads is exactly the length of p's chunk. */
        size_t gap = (size_t) ((char *) q - (char *) p);
        if (gap != cases[i].chunk) {
            printf ("FAIL: request %zu consumed %zu bytes, expected %zu\n",
                    cases[i].asked, gap, cases[i].chunk);
            failures++;
        }

        free (p);
        free (q);
    }

    /* A request bigger than the bank must fail, and must fail quietly
     * enough to leave the bank usable. */
    char *too_big = malloc (MEMLENGTH + 1);
    if (too_big != NULL) {
        printf ("FAIL: an oversized request returned a pointer: %p\n", (void *) too_big);
        failures++;
    }

    if (failures) {
        printf ("%d alignment check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf ("all alignment checks passed\n");
    return EXIT_SUCCESS;
}
