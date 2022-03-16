fizz: fizz.c
	clang -o fizz fizz.c -O3

.PHONY: run
run: fizz
	./fizz | pv --force > /dev/null

.PHONY: output
output: fizz
	./fizz
