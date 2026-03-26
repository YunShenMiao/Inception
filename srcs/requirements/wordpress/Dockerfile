FROM alpine:3.22

COPY ./tools/init.sh /
 # COPY ./tools/wp-config.php /

RUN apk add --no-cache php php-fpm php-mysqli curl tar
RUN curl -o /tmp/wordpress.tar.gz https://wordpress.org/latest.tar.gz && \
    tar -xzf /tmp/wordpress.tar.gz -C /var/www && \
    rm /tmp/wordpress.tar.gz

EXPOSE 9000

ENTRYPOINT ["/init.sh"]
CMD ["php-fpm", "-F"]