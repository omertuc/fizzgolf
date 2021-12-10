fizzbuzz: fizzbuzz.o
	ld -o fizzbuzz fizzbuzz.o

fizzbuzz.o: fizzbuzz.S
	gcc -mavx2 -c fizzbuzz.S

.PHONY: run
run: fizzbuzz
	taskset 1 ./fizzbuzz | taskset 2 pv --force > /dev/null
