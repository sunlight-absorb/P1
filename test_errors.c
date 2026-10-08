/* Leak reports from children that exit with live allocations are expected. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "mymalloc.h"

#ifndef MEMLENGTH
#define MEMLENGTH 4096
#endif

static void bad_foreign (void)
{
    int x = 0;
    free (&x);
}

static void bad_interior (void)
{
    int *p = malloc (sizeof (int) * 2);
    free (p + 1);
}

static void bad_double (void)
{
    int *p = malloc (sizeof (int) * 100);
    int *q = p;
    free (p);
    free (q);
}

static void bad_null (void)
{
    free (NULL);
}

static int child_status (void (*scenario) (void))
{
    /* Flush before fork so child exit() does not reprint the parent's output. */
    fflush (stdout);
    fflush (stderr);

    pid_t child = fork ();
    if (child < 0)
        return -1;
    if (child == 0) {
        scenario ();
        exit (0);
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

    passed += expect_exit2 ("free(&x)", bad_foreign);
    passed += expect_exit2 ("free(p + 1)", bad_interior);
    passed += expect_exit2 ("free(q) after free(p)", bad_double);
    passed += expect_exit2 ("free(NULL)", bad_null);
    passed += expect_null ();

    if (passed < 5) {
        printf ("%d of 5 error checks passed\n", passed);
        return EXIT_FAILURE;
    }
    printf ("all 5 error checks passed\n");
    return EXIT_SUCCESS;
}
