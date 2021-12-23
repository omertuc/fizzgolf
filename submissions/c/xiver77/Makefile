fizz: fizz.c
	gcc -std=c11 -Ofast -mtune=native -mssse3 -fwhole-program -s -o fizz fizz.c

.PHONY: run
run: fizz
	./fizz | pv --force > /dev/null

