#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mymalloc.h"

#define HEAPSIZE   4096
#define HEADERSIZE 8

int failures = 0;

void check(int ok, const char *name)
{
    printf("%s: %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok)
        failures++;
}

/* Requirement 1: malloc() reserves unallocated memory.
 * Fill four large objects with different bytes. If two objects overlap,
 * one object's bytes change when the other object is filled. */
void test_no_overlap(void)
{
    char *obj[4];
    int ok = 1;

    for (int i = 0; i < 4; i++) {
        obj[i] = malloc(1000);
        if (obj[i] == NULL)
            ok = 0;
    }

    if (ok) {
        for (int i = 0; i < 4; i++)
            memset(obj[i], i + 1, 1000);

        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 1000; j++)
                if (obj[i][j] != i + 1)
                    ok = 0;
    }

    for (int i = 0; i < 4; i++)
        if (obj[i] != NULL)
            free(obj[i]);

    check(ok, "objects do not overlap");
}

/* Requirement 2: free() deallocates memory.
 * Only one 3000-byte object fits in the heap. If free() did not give the
 * memory back, the second malloc() in this loop would fail. */
void test_free_deallocates(void)
{
    int ok = 1;

    for (int i = 0; i < 10; i++) {
        char *p = malloc(3000);
        if (p == NULL) {
            ok = 0;
            break;
        }
        free(p);
    }

    check(ok, "freed memory can be allocated again");
}

/* Requirement 3: adjacent free chunks are coalesced.
 * Fill the heap with four 1000-byte objects, then free the two in the
 * middle. A 2000-byte request fits only if those two chunks merged. */
void test_coalesce(void)
{
    char *a = malloc(1000);
    char *b = malloc(1000);
    char *c = malloc(1000);
    char *d = malloc(1000);

    if (a == NULL || b == NULL || c == NULL || d == NULL) {
        check(0, "set up the coalescing test");
        return;
    }

    free(b);
    free(c);

    char *big = malloc(2000);
    check(big != NULL, "two adjacent free chunks merge");

    // Free everything so the whole heap must be one free chunk again so the largest possible request must succeed
    if (big != NULL)
        free(big);
    free(d);
    free(a);

    char *whole = malloc(HEAPSIZE - HEADERSIZE);
    check(whole != NULL, "all free chunks merge into the whole heap");
    if (whole != NULL)
        free(whole);
}

int main(void)
{
    test_free_deallocates();
    test_no_overlap();
    test_coalesce();

    if (failures > 0) {
        printf("%d test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("all tests passed\n");
    return EXIT_SUCCESS;
}
