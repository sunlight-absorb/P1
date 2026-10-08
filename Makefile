CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11 -g -O2
LIB      = mymalloc.c
HDR      = mymalloc.h

PROGRAMS = memgrind memtest test_coalesce test_free_reuse test_alignment test_errors test_leak

all: $(PROGRAMS)

.PHONY: all check clean

%: %.c $(LIB) $(HDR)
	$(CC) $(CFLAGS) -o $@ $< $(LIB)

memtest-real: memtest.c
	$(CC) $(CFLAGS) -DREALMALLOC -o memtest-real memtest.c

memtest-leak: memtest.c $(LIB) $(HDR)
	$(CC) $(CFLAGS) -DLEAK -o memtest-leak memtest.c $(LIB)

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
