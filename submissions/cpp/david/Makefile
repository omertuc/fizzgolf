fb: fb.cc
	g++ fb.cc -march=native -o fb -O3 -std=c++20 -Wall -fno-tree-vectorize -fno-exceptions

.PHONY: run
run: fb
	taskset -c 0-6 sudo ./fb | taskset -c 7 ~/repos/pv/pv -B 2M --force > /dev/null

.PHONY: output
output: fb
	./fb
