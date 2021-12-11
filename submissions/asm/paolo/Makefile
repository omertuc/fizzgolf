fb: fb.c
	gcc fb.c -o fb -O2 -g -mavx -mavx2 -flax-vector-conversions

.PHONY: run
run: fb
	taskset 1 ./fb | taskset 2 pv --force > /dev/null
