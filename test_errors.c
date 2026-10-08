/* test_errors.c: runs one error or leak scenario, chosen by the argument.
 *
 * These scenarios end the program or print at exit, so each one runs as a
 * separate program. "make check" runs each scenario and checks the result.
 *
 *   ./test_errors leak      expect "mymalloc: 48 bytes leaked in 2 objects."
 *   ./test_errors noleak    expect no output
 *   ./test_errors outside   expect a free() error and exit status 2
 *   ./test_errors interior  expect a free() error and exit status 2
 *   ./test_errors double    expect a free() error and exit status 2
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mymalloc.h"

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("usage: %s leak|noleak|outside|interior|double\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *mode = argv[1];

    if (strcmp(mode, "leak") == 0) {
        /* Requirement 4: leaked objects are detected and reported.
         * Keep a 16-byte and a 32-byte object; free the third object.
         * The report must count only the two objects that remain. */
        char *a = malloc(16);
        char *b = malloc(8);
        char *c = malloc(32);
        free(b);
        (void)a;
        (void)c;
    } else if (strcmp(mode, "noleak") == 0) {
        /* A program that frees everything must not get a leak report. */
        char *a = malloc(16);
        char *b = malloc(32);
        free(a);
        free(b);
    } else if (strcmp(mode, "outside") == 0) {
        /* Requirement 5: free() detects pointers it did not return. */
        int x;
        free(&x);
    } else if (strcmp(mode, "interior") == 0) {
        char *p = malloc(16);
        free(p + 1);
    } else if (strcmp(mode, "double") == 0) {
        char *p = malloc(16);
        free(p);
        free(p);
    } else {
        printf("unknown scenario: %s\n", mode);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
