.PHONY: ./target/release/xavier
./target/release/xavier:
	cargo build --release

.PHONY: run
run: ./target/release/xavier
	./target/release/xavier | pv --force > /dev/null

.PHONY: output
output: ./target/release/xavier
	./target/release/xavier
