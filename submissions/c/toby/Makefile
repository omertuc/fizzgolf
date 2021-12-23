fizz: fizz.c
	gcc -std=gnu17 -Wall -Wextra -fopenmp -O3 -march=native -o fizz fizz.c

.PHONY: run
run: fizz
	OMP_NUM_THREADS=3 ./fizz | pv --force > /dev/null

.PHONY: output
output: fizz
	OMP_NUM_THREADS=3 ./fizz
