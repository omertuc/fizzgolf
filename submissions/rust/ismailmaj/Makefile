.PHONY: ./target/release/high-throughput-fizzbuzz
./target/release/high-throughput-fizzbuzz:
	cargo build --release

.PHONY: run
run: ./target/release/high-throughput-fizzbuzz
	./target/release/high-throughput-fizzbuzz | pv --force > /dev/null

.PHONY: output
output: ./target/release/high-throughput-fizzbuzz
	./target/release/high-throughput-fizzbuzz
