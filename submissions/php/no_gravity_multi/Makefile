.PHONY: docker_build
docker_build:
	docker build . -t php

.PHONY: run
run: docker_build
	docker run -v $(CURDIR):/work:Z -it --rm --log-driver=none php bash -c 'php -d opcache.enable_cli=1 -d opcache.jit_buffer_size=64M work/fizzbuzz.php | pv --force > /dev/null'

.PHONY: output
output: docker_build
	docker run -v $(CURDIR):/work:Z -it --rm --log-driver=none php php -d opcache.enable_cli=1 -d opcache.jit_buffer_size=64M work/fizzbuzz.php
