all: clean test_helpers

test_helpers:
	gcc -std=c99 -o test test.c helpers.h helpers.c

clean:
	rm -f test
