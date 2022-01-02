fizz: fizz.c
	gcc -std=c11 -Ofast -mtune=native -mssse3 -fwhole-program -s -o fizz fizz.c

.PHONY: run
run: fizz
	taskset 1 ./fizz | taskset 2 pv --force > /dev/null

.PHONY: output
output: fizz
	./fizz
