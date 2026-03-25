#BASE
FROM alpine:3.22

# COPY SRC DEST
COPY conf/my.cnf /etc/mysql/my.cnf
COPY tools/init.sh /init.sh

# EXECUTE BUILD CMDS TO CREATE NEW LAYER 
RUN chmod +x /init.sh
RUN apk add --no-cache mariadb mariadb-client
RUN mkdir -p /var/lib/mysql

# CONTAINER LISTENS ON
EXPOSE 3306

#default commands (can be overwritten by docker run arguments)
CMD ["mysqld", "--datadir=/var/lib/mysql", "--bind-address=0.0.0.0"]

#default executable
ENTRYPOINT ["/init.sh"]
