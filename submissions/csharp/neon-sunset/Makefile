.PHONY: ./publish/FizzBuzz
./publish/FizzBuzz:
	dotnet publish -c release -o publish

.PHONY: run
run: ./publish/FizzBuzz
	./publish/FizzBuzz | pv --force > /dev/null

.PHONY: output
output: ./publish/FizzBuzz
	./publish/FizzBuzz