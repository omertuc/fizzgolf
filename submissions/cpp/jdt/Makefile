Johan: a.cpp
	g++ a.cpp -lpthread -o Johan -O3

.PHONY: run
run: Johan
	./Johan | pv --force > /dev/null
	
.PHONY: output
output: Johan
	./Johan
