fizz: fizz.cpp
	g++ -std=c++2a -Wall -Wextra -Weffc++ -DNDEBUG -O3 -march=native -fno-exceptions -lpthread fizz.cpp -o fizz

.PHONY: run
run: fizz
	./fizz | pv --force > /dev/null
	
.PHONY: output
output: fizz
	./fizz
