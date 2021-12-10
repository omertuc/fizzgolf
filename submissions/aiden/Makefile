./target/release/fizz_buzz:
	cargo build --release

.PHONY: run
run: ./target/release/fizz_buzz
	./target/release/fizz_buzz | pv --force > /dev/null
