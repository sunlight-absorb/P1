/*
 * Requirement: free() returns space to the pool, and that space is reusable
 * without disturbing live neighbours.
 *
 * Detection: after freeing the middle of three adjacent objects, an
 * identically sized request must be served from that hole (first fit finds
 * it before the untouched tail of the bank), and writing through the new
 * object must leave the neighbours' byte patterns intact.
 *
 * Test: allocate three objects, fill each with a distinct pattern, free the
 * middle one, allocate again, and check both the address and the patterns.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mymalloc.h"

#define OBJ 128

static int same (char *p, int pattern, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (p[i] != pattern)
            return 0;
    return 1;
}

int
main (void)
{
    int failures = 0;

    char *a = malloc (OBJ);
    char *b = malloc (OBJ);
    char *c = malloc (OBJ);
    if (a == NULL || b == NULL || c == NULL) {
        printf ("FAIL: could not set up the test\n");
        return EXIT_FAILURE;
    }

    memset (a, 0x11, OBJ);
    memset (b, 0x22, OBJ);
    memset (c, 0x33, OBJ);

    free (b);

    char *d = malloc (OBJ);
    if (d == NULL) {
        printf ("FAIL: freed space was not offered again\n");
        return EXIT_FAILURE;
    }
    if (d != b) {
        printf ("FAIL: reuse used %p instead of the freed chunk %p\n",
                (void *) d, (void *) b);
        failures++;
    }
    if (d == a || d == c) {
        printf ("FAIL: reuse handed out a live object\n");
        failures++;
    }

    memset (d, 0x44, OBJ);

    if (!same (a, 0x11, OBJ)) {
        printf ("FAIL: writing to the reused chunk clobbered the object before it\n");
        failures++;
    }
    if (!same (c, 0x33, OBJ)) {
        printf ("FAIL: writing to the reused chunk clobbered the object after it\n");
        failures++;
    }

    free (a);
    free (d);
    free (c);

    if (failures) {
        printf ("%d free/reuse check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf ("all 4 free/reuse checks passed\n");
    return EXIT_SUCCESS;
}
