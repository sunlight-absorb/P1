/* The leaked 10- and 30-byte requests round to 16 + 32 = 48 payload bytes. */
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

    free (b);

    printf ("expected on stderr: mymalloc: 48 bytes leaked in 2 objects.\n");
    return EXIT_SUCCESS;
}
