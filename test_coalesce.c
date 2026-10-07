/*
 * Requirement: free() coalesces adjacent free chunks, and malloc() can use
 * the merged space.
 *
 * Detection: the chunks partition the bank, so a request larger than any
 * single object can only be satisfied from space that was merged. The
 * returned pointer must also land inside the region that was freed.
 *
 * Test: allocate five objects, free the middle three, then ask for an
 * object larger than three of them. First fit can only answer it from the
 * coalesced hole, whose start is the first freed object. Then free the rest
 * and ask for the whole bank, which only fits if every chunk merged back
 * into one; that request must be served at the very start of the bank, the
 * address the first object of this run occupies.
 */

#include <stdio.h>
#include <stdlib.h>

#include "mymalloc.h"

/* The bank this test runs against; build with -DMEMLENGTH=... to change it,
 * which resizes the library and this test together. */
#ifndef MEMLENGTH
#define MEMLENGTH 4096
#endif

#define N         5
#define OBJ       64
#define HEADERSIZE 8

int
main (void)
{
    char *obj[N];
    int failures = 0;

    for (int i = 0; i < N; i++) {
        obj[i] = malloc (OBJ);
        if (obj[i] == NULL) {
            printf ("FAIL: object %d was not allocated\n", i);
            return EXIT_FAILURE;
        }
    }

    /* Three adjacent objects become one hole of 3 * (OBJ + header) bytes. */
    free (obj[1]);
    free (obj[2]);
    free (obj[3]);

    /* 192 payload bytes need 200 bytes of chunk: too big for one 72-byte
     * chunk, small enough for the merged 216-byte hole. */
    char *big = malloc (3 * OBJ);
    if (big == NULL) {
        printf ("FAIL: coalesced space was not usable\n");
        failures++;
    } else if (big != obj[1]) {
        printf ("FAIL: merged space started at %p, expected %p\n",
                (void *) big, (void *) obj[1]);
        failures++;
    }

    /* Everything is free now, so the bank must be one chunk again: the
     * largest request it can serve is the bank minus one header. */
    free (obj[0]);
    free (obj[4]);
    free (big);

    char *whole = malloc (MEMLENGTH - HEADERSIZE);
    if (whole == NULL) {
        printf ("FAIL: the whole bank was not coalesced into one chunk\n");
        failures++;
    } else if (whole != obj[0]) {
        /* A fresh bank starts with its header, so the first payload of this
         * run marks the start of the bank. */
        printf ("FAIL: whole-bank chunk started at %p, expected %p\n",
                (void *) whole, (void *) obj[0]);
        failures++;
    } else {
        free (whole);
    }

    if (failures) {
        printf ("%d coalescing check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf ("all 3 coalescing checks passed\n");
    return EXIT_SUCCESS;
}
