#BASE
FROM alpine:3.22

COPY html/ /usr/share/nginx/html/
COPY conf/nginx.conf /etc/nginx/nginx.conf
COPY tools/init.sh /init.sh

RUN apk add --no-cache nginx
RUN mkdir -p /run/nginx

EXPOSE 443

CMD ["nginx", "-g", "daemon off;"]

ENTRYPOINT ["/init.sh"]