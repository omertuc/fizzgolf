FROM php:8.1-cli

RUN apt-get update

RUN apt-get install -y pv

RUN	docker-php-ext-install -j$(nproc) sysvsem sysvshm sysvshm pcntl opcache
#RUN	docker-php-ext-install -j$(nproc) sysvsem
#RUN	docker-php-ext-install -j$(nproc) sysvshm
#RUN	docker-php-ext-install -j$(nproc) pcntl
#RUN	docker-php-ext-install -j$(nproc) opcache
