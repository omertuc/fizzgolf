.PHONY: docker_build
docker_build:
	docker build . -t php

.PHONY: run
run: docker_build
	docker run -v $(PWD):/work php php /work/fizzbuzz.php | pv --force > /dev/null

.PHONY: output
output: docker_build
	docker run -v $(PWD):/work php php /work/fizzbuzz.php
