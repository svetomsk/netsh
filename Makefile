all: clean main

main:
	gcc -std=gnu99 -o netsh netsh.c helpers.h helpers.c

test_helpers:
	gcc -std=c99 -o test test.c helpers.h helpers.c

clean:
	rm -f test
