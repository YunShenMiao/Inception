FROM alpine:3.22

COPY ./tools/init.sh /

RUN apk add --no-cache php83 php83-ctype php83-fpm php83-mysqli php83-phar php83-iconv php83-tokenizer php83-xml php83-mbstring php83-dom php83-simplexml curl tar mariadb-client redis php83-pecl-redis \
    && chmod +x /init.sh \ 
    && mkdir -p /var/www/html \
    && curl -o /tmp/wordpress.tar.gz https://wordpress.org/latest.tar.gz \
    && tar -xzf /tmp/wordpress.tar.gz -C /var/www/html --strip-components=1 \
    && rm /tmp/wordpress.tar.gz \
    && curl -o /usr/local/bin/wp https://raw.githubusercontent.com/wp-cli/builds/gh-pages/phar/wp-cli.phar \
    && chmod +x /usr/local/bin/wp \
    && chown -R nobody:nobody /var/www/html \
    && sed -i 's/listen = 127.0.0.1:9000/listen = 0.0.0.0:9000/' /etc/php83/php-fpm.d/www.conf


EXPOSE 9000

ENTRYPOINT ["/init.sh"]
CMD ["php-fpm83", "-F"]