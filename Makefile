# Makefile for P1: my little malloc()
#
# make          builds memgrind and every test program
# make check    builds, then runs the tests and prints a summary
# make clean    removes the executables
#
# Every program is a separate .c file linked against mymalloc.c; the macros in
# mymalloc.h turn malloc()/free() into calls that carry file and line numbers.

CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11 -g -O2
LIB      = mymalloc.c
HDR      = mymalloc.h

PROGRAMS = memgrind memtest test_coalesce test_free_reuse test_alignment test_errors test_leak

all: $(PROGRAMS)

.PHONY: all check clean

# One program per .c file, plus the library.
%: %.c $(LIB) $(HDR)
	$(CC) $(CFLAGS) -o $@ $< $(LIB)

# memtest against the real libc allocator, for comparison.
memtest-real: memtest.c
	$(CC) $(CFLAGS) -DREALMALLOC -o memtest-real memtest.c

# memtest that leaks on purpose, to exercise the leak report.
memtest-leak: memtest.c $(LIB) $(HDR)
	$(CC) $(CFLAGS) -DLEAK -o memtest-leak memtest.c $(LIB)

# Run every test, keep going past a failure, and exit nonzero if any failed.
check: all memtest-real memtest-leak
	@fails=0; \
	echo '--- correct traffic ---'; \
	for t in memtest memtest-real test_coalesce test_free_reuse test_alignment; do \
		./$$t || fails=$$((fails + 1)); \
	done; \
	echo '--- deliberate misuse: error messages below are expected ---'; \
	for t in memtest-leak test_leak test_errors; do \
		./$$t || fails=$$((fails + 1)); \
	done; \
	echo '--- stress ---'; \
	./memgrind || fails=$$((fails + 1)); \
	if [ $$fails -ne 0 ]; then echo "check: $$fails test program(s) failed"; exit 1; fi; \
	echo 'check: all test programs passed'

clean:
	rm -f $(PROGRAMS) memtest-real memtest-leak
