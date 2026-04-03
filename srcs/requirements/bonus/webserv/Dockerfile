#FROM alpine:3.22

#RUN apk add --no-cache libstdc++

#COPY /tools/webserv /usr/local/bin/webserv
#COPY /conf/default.conf /etc/default.conf
#COPY /tools/www /var/www

#EXPOSE 8080 9110 7110

#ENTRYPOINT ["/usr/local/bin/webserv"]

#CMD ["/etc/default.conf"]

FROM alpine:3.22 AS builder

RUN apk add --no-cache g++ make

WORKDIR /tools
COPY tools/ .
RUN ls -l
RUN make

FROM alpine:3.22

RUN apk add --no-cache libstdc++

COPY --from=builder /tools/webserv /usr/local/bin/webserv
COPY conf/default.conf /etc/default.conf
COPY www /var/www

ENTRYPOINT ["/usr/local/bin/webserv"]
CMD ["/etc/default.conf"]