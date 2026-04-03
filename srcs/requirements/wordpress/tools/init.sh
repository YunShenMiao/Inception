#!/bin/sh
set -ex

cd /var/www/html

DB_PASSWORD=$(cat /run/secrets/db_password)

until mariadb-admin ping -h "${MYSQL_HOST}" -u "${MYSQL_USER}" --password="${DB_PASSWORD}" --silent 2>/dev/null; do
    echo "Waiting for MariaDB..."
    sleep 1
done

if [ ! -f wp-config.php ]; then
    echo "Creating wp-config.php..."

    SALTS=$(curl -s https://api.wordpress.org/secret-key/1.1/salt/)

    cat <<EOF > wp-config.php
<?php
define( 'DB_NAME', '${MYSQL_DATABASE}' );
define( 'DB_USER', '${MYSQL_USER}' );
define( 'DB_PASSWORD', '${DB_PASSWORD}' );
define( 'DB_HOST', '${MYSQL_HOST}' );
define( 'DB_CHARSET', 'utf8' );
define( 'DB_COLLATE', '' );
define( 'WP_REDIS_HOST', 'redis_in' );
define( 'WP_REDIS_PORT', 6379 );
define( 'WP_CACHE', true );

EOF

    echo "$SALTS" >> wp-config.php

    cat <<'EOF' >> wp-config.php

$table_prefix = 'wp_';
define( 'WP_DEBUG', false );
if ( !defined('ABSPATH') )
    define('ABSPATH', dirname(__FILE__) . '/');
require_once(ABSPATH . 'wp-settings.php');
EOF

fi

if ! wp core is-installed --allow-root; then
    WP_ADMIN_PASSWORD=$(cat /run/secrets/wp_admin_password)
    WP_USER_PASSWORD=$(cat /run/secrets/wp_user_password)

    wp core install --allow-root \
        --url="https://${DOMAIN_NAME}" \
        --title="Inception" \
        --admin_user="${WP_ADMIN_USER}" \
        --admin_password="${WP_ADMIN_PASSWORD}" \
        --admin_email="${WP_ADMIN_EMAIL}" \
        --skip-email

    wp user create --allow-root \
        "${WP_USER}" "${WP_USER_EMAIL}" \
        --role=subscriber \
        --user_pass="${WP_USER_PASSWORD}"
fi

    wp plugin install redis-cache --activate --allow-root
    wp redis enable --allow-root

exec "$@"