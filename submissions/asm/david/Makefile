fb: fb.cc
	g++ fb.cc -march=native -o fb -O3 -Wall -std=c++20 -fopenmp

.PHONY: run
run: fb
	taskset -c 0-2 sudo ./fb | taskset -c 3 pv --force > /dev/null


.PHONY: output
output: fb
	./fb
