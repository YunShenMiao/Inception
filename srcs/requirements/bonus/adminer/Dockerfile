From alpine:3.22

RUN apk add --no-cache php php-mysqli php-session

WORKDIR /var/www/html

RUN wget -q https://github.com/vrana/adminer/releases/download/v5.4.2/adminer-5.4.2.php \
        -O adminer.php

EXPOSE 8080

CMD ["php", "-S", "0.0.0.0:8080", "-t", "/var/www/html"]