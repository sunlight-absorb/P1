/*
 * Requirement: objects still allocated when the process ends are reported,
 * with their total payload size and their count.
 *
 * Detection: leak a known set of objects and compare the report the library
 * prints at exit with the expected line, which this program prints first so
 * the two can be read side by side.
 *
 * Test: allocate three objects of 10, 20 and 30 bytes, free the middle one,
 * and let the other two leak. Their payloads round to 16 and 32 bytes, so the
 * report should name 48 bytes in 2 objects.
 */

#include <stdio.h>
#include <stdlib.h>

#include "mymalloc.h"

int
main (void)
{
    char *a = malloc (10);
    char *b = malloc (20);
    char *c = malloc (30);

    if (a == NULL || b == NULL || c == NULL) {
        printf ("FAIL: could not set up the leak test\n");
        return EXIT_FAILURE;
    }

    free (b); /* only a and c leak */

    printf ("expected on stderr: mymalloc: 48 bytes leaked in 2 objects.\n");
    return EXIT_SUCCESS;
}
