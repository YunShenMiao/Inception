#BASE
FROM alpine:3.22

# COPY SRC DEST
COPY conf/my.cnf /etc/mysql/my.cnf
COPY tools/init.sh /init.sh

# EXECUTE BUILD CMDS TO CREATE NEW LAYER 
RUN apk add --no-cache mariadb mariadb-client \
    && mkdir -p /var/lib/mysql \
    && chmod +x /init.sh

# CONTAINER LISTENS ON
EXPOSE 3306

#default executable
ENTRYPOINT ["/init.sh"]

#default arguments (can be overwritten by docker run arguments)
#mysqld: runs database -> starts database engine, loads my.cnf & initializes storage engines
CMD ["mysqld"]