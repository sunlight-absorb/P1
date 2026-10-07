/*
 * Requirement: the misuse listed in section 2.1 is reported to standard
 * error, and the process then terminates with exit status 2.
 *
 * Detection: exit(2) ends the process, so each scenario runs in its own
 * child; the parent only has to confirm the status the child died with. The
 * message itself is written to standard error where it can be read.
 *
 * Test: the three scenarios from the writeup (a pointer that was never
 * allocated, a pointer into the middle of a chunk, and a pointer freed
 * twice), each of which must exit with 2; plus free(NULL), which standard C
 * makes a no-op and which must not stop the process; plus an oversized
 * malloc, which must return NULL with a message and keep running.
 *
 * A child that leaves an object allocated also prints the leak report when
 * it exits; that line is correct behaviour, not a failure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "mymalloc.h"

#ifndef MEMLENGTH
#define MEMLENGTH 4096
#endif

/* free() on the address of a local: never obtained from malloc(). */
static void bad_foreign (void)
{
    int x = 0;
    free (&x);
}

/* free() one int past the start of a chunk. */
static void bad_interior (void)
{
    int *p = malloc (sizeof (int) * 2);
    free (p + 1);
}

/* free() the same object twice. */
static void bad_double (void)
{
    int *p = malloc (sizeof (int) * 100);
    int *q = p;
    free (p);
    free (q);
}

/* free() of NULL: standard C makes this a no-op. */
static void null_noop (void)
{
    free (NULL);
}

/* Run scenario in a fresh child and return the status it exited with, or -1
 * if the fork/wait failed or the child died by signal. stdio is flushed
 * first: the child inherits the parent's buffers, and exit() flushes them,
 * so unflushed PASS lines would otherwise be printed again once per child. */
static int child_status (void (*scenario) (void))
{
    fflush (stdout);
    fflush (stderr);

    pid_t child = fork ();
    if (child < 0)
        return -1;
    if (child == 0) {
        scenario ();
        exit (0); /* only reached when the library missed an error */
    }

    int status;
    if (waitpid (child, &status, 0) == -1)
        return -1;
    if (!WIFEXITED (status))
        return -1;
    return WEXITSTATUS (status);
}

static int expect_exit2 (const char *name, void (*scenario) (void))
{
    int status = child_status (scenario);

    if (status == -1) {
        printf ("FAIL: could not run the %s child\n", name);
        return 0;
    }
    if (status == 2) {
        printf ("PASS: %s reported an error and exited with 2\n", name);
        return 1;
    }
    printf ("FAIL: %s exited with %d, expected 2\n", name, status);
    return 0;
}

/* free(NULL) is not one of the errors section 2.1 requires, so the child has
 * to come back on its own. */
static int expect_noop (const char *name, void (*scenario) (void))
{
    int status = child_status (scenario);

    if (status == -1) {
        printf ("FAIL: could not run the %s child\n", name);
        return 0;
    }
    if (status == 0) {
        printf ("PASS: %s returned normally\n", name);
        return 1;
    }
    printf ("FAIL: %s exited with %d, expected a normal return\n", name, status);
    return 0;
}

/* An impossible request must return NULL, print a message, and leave the
 * process running. */
static int expect_null (void)
{
    char *p = malloc (MEMLENGTH + 1);
    if (p != NULL) {
        printf ("FAIL: an impossible request returned %p\n", (void *) p);
        return 0;
    }
    printf ("PASS: an impossible request returned NULL\n");
    return 1;
}

int
main (void)
{
    int passed = 0;

    /* Nothing has been allocated yet, so each child starts from a clean
     * bank and registers its own leak detector. */
    passed += expect_exit2 ("free(&x)", bad_foreign);
    passed += expect_exit2 ("free(p + 1)", bad_interior);
    passed += expect_exit2 ("free(q) after free(p)", bad_double);
    passed += expect_noop ("free(NULL)", null_noop);
    passed += expect_null ();

    if (passed < 5) {
        printf ("%d of 5 error checks passed\n", passed);
        return EXIT_FAILURE;
    }
    printf ("all 5 error checks passed\n");
    return EXIT_SUCCESS;
}
