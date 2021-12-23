FizzBuzz.class: FizzBuzz.java
	javac FizzBuzz.java

.PHONY: run
run: FizzBuzz.class
	java FizzBuzz | pv --force > /dev/null

.PHONY: output
output: FizzBuzz.class
	java FizzBuzz
