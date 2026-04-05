FROM alpine:3.22

RUN apk add --no-cache busybox-extras

COPY html/ /www/

EXPOSE 8081

CMD ["httpd", "-f", "-p", "8081", "-h", "/www"]