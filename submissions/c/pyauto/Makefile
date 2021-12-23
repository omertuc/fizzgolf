fizz: fizz.c
	clang -Ofast fizz.c -o fizz

.PHONY: run
run: fizz
	./fizz | pv --force > /dev/null

.PHONY: output
output: fizz
	./fizz
