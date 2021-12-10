fizzbuzz: fizz.c
	clang -o fizz fizz.c -O3 -march=native

.PHONY: run
run: fizz
	./fizz | pv --force > /dev/null
