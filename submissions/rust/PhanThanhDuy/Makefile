./target/release/fizzbuzz:
	cargo build --release

.PHONY: run
run: ./target/release/fizzbuzz
	./target/release/fizzbuzz | pv --force > /dev/null

.PHONY: output
output: ./target/release/fizzbuzz
	./target/release/fizzbuzz
