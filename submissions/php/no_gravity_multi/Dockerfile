FROM php:8.1-zts-buster

RUN apt-get update

RUN apt-get install -y \
		zlib1g-dev \
		libpng-dev \
		libfreetype6-dev \
		libjpeg62-turbo-dev

RUN	docker-php-ext-configure gd --with-freetype --with-jpeg

RUN	docker-php-ext-install -j$(nproc) sysvsem
RUN	docker-php-ext-install -j$(nproc) sysvshm
RUN	docker-php-ext-install -j$(nproc) sysvmsg
RUN	docker-php-ext-install -j$(nproc) pcntl
