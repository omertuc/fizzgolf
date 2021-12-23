fizz: fizz.c
	gcc -O3 fizz.c -o fizz

.PHONY: run
run: fizz
	./fizz | pv --force > /dev/null

.PHONY: output
output: fizz
	./fizz
