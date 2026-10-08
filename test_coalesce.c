#include <stdio.h>
#include <stdlib.h>

#include "mymalloc.h"

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

    free (obj[0]);
    free (obj[4]);
    free (big);

    char *whole = malloc (MEMLENGTH - HEADERSIZE);
    if (whole == NULL) {
        printf ("FAIL: the whole bank was not coalesced into one chunk\n");
        failures++;
    } else if (whole != obj[0]) {
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
