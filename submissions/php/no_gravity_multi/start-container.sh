#!/bin/sh
trap 'docker kill no_gravity_multi' EXIT
docker run -v `pwd`:/work:Z --name no_gravity_multi -it --rm -log-driver=none php bash -c 'php -d opcache.enable_cli=1 -d opcache.jit_buffer_size=64M work/fizzbuzz.php | pv > /dev/null' >&2
