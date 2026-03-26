FROM alpine:3.22

COPY ./tools/init.sh /

RUN apk add --no-cache php php-fpm php-mysqli php-phar curl tar mariadb-client \
    && chmod +x /init.sh \ 
    && mkdir -p /var/www/html \
    && curl -o /tmp/wordpress.tar.gz https://wordpress.org/latest.tar.gz \
    && tar -xzf /tmp/wordpress.tar.gz -C /var/www/html --strip-components=1 \
    && rm /tmp/wordpress.tar.gz \
    && curl -o /usr/local/bin/wp https://raw.githubusercontent.com/wp-cli/builds/gh-pages/phar/wp-cli.phar \
    && chmod +x /usr/local/bin/wp \
    && chown -R nobody:nobody /var/www/html


EXPOSE 9000

ENTRYPOINT ["/init.sh"]
CMD ["php-fpm", "-F"]