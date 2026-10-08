CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11 -g -O2
LIB      = mymalloc.c
HDR      = mymalloc.h

PROGRAMS = memgrind memtest tests test_errors

all: $(PROGRAMS)

.PHONY: all check clean

%: %.c $(LIB) $(HDR)
	$(CC) $(CFLAGS) -o $@ $< $(LIB)

# Each line stops "make check" if its test fails.
check: all
	./memtest
	./tests
	@echo '--- leak detection ---'
	./test_errors leak 2>&1 | grep 'mymalloc: 48 bytes leaked in 2 objects.'
	test -z "$$(./test_errors noleak 2>&1)"
	@echo '--- bad free() calls: each must print an error and exit with 2 ---'
	./test_errors outside;  test $$? -eq 2
	./test_errors interior; test $$? -eq 2
	./test_errors double;   test $$? -eq 2
	@echo '--- performance ---'
	./memgrind
	@echo 'check: all tests passed'

clean:
	rm -f $(PROGRAMS)
