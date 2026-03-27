#BASE
FROM alpine:3.22

#COPY html/ /usr/share/nginx/html/
COPY conf/nginx.conf /etc/nginx/nginx.conf

RUN apk add --no-cache nginx openssl \
    && mkdir -p /etc/nginx/ssl \
    && openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
       -keyout /etc/nginx/ssl/nginx.key \
       -out /etc/nginx/ssl/nginx.crt \
       -subj "/CN=jwardeng.42.fr"


EXPOSE 443

CMD ["nginx", "-g", "daemon off;"]