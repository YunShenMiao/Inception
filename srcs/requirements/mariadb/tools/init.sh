#!/bin/sh

# do i need --socket still? or does it get it from config

# sets -e -> exit on error
set -ex
unset MYSQL_HOST

# if directory doesnt exist (!system database) init with mariadb-install-db
if [ ! -d "/var/lib/mysql/mysql" ]; then
    echo "Initializing database..."
    mariadb-install-db --user=mysql --datadir=/var/lib/mysql

# initial server startup + prevent connection while initializing,[& runs in background] [$! = PID of MariaDB server]
    mariadbd --socket=/var/lib/mysql/mysql.sock --user=mysql &
    pid="$!"

# Wait until server is ready ([until = while !] 0=server ready, non-zero=not ready)
# --socket=/var/lib/mysql/mysql.sock
    until mysqladmin ping --silent; do
        echo "Waiting for MariaDB to start..."
        sleep 1
    done

    MYSQL_PASSWORD=$(cat /run/secrets/db_password)
    MYSQL_ROOT_PASSWORD=$(cat /run/secrets/db_root_password)

#sending SQL commands to mysql client (mysql = mariadb command line client, -u root -> connect as root)
# --socket=/var/lib/mysql/mysql.sock
    mysql --socket=/var/lib/mysql/mysql.sock -u root << EOF
CREATE DATABASE IF NOT EXISTS ${MYSQL_DATABASE};
CREATE USER IF NOT EXISTS '${MYSQL_USER}'@'%' IDENTIFIED BY '${MYSQL_PASSWORD}';
GRANT ALL PRIVILEGES ON ${MYSQL_DATABASE}.* TO '${MYSQL_USER}'@'%';
ALTER USER 'root'@'localhost' IDENTIFIED BY '${MYSQL_ROOT_PASSWORD}';
FLUSH PRIVILEGES;
EOF

    kill "$pid"
    wait "$pid"
fi

    chown -R mysql:mysql /var/lib/mysql
    exec "$@" --user=mysql --datadir=/var/lib/mysql