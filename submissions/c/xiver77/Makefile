fizz: fizz.c
	gcc -s -O3 -msse4.1 -fwhole-program -ofizz fizz.c

.PHONY: run
run: fizz
	taskset 1 ./fizz | taskset 2 pv --force > /dev/null

.PHONY: output
output: fizz
	./fizz
