# User Documentation — Inception

---

## Services provided

| Service | What it does | How to access |
| --- | --- | --- |
| WordPress | The main website and blog platform | [https://jwardeng.42.fr](https://jwardeng.42.fr/) |
| Adminer | UI for database management | [http://localhost:8080](http://localhost:8080/) |
| Static site | static HTML page | http://localhost/static/ |
| CatSurf | Custom HTTP server with file upload and download | [http://localhost:9110](http://localhost:9110/) + http://localhost:9090 |
| MariaDB | Database  | internal only |
| Redis | Cache | internal only |
| Nginx | web server | [https://jwardeng.42.fr](https://jwardeng.42.fr/) |

---

## Initializing the project:

Create your secrets (never commit these):

```bash
mkdir -p secrets
echo "your_db_password" > mysql_pw
echo "your_db_root_password" > secrets/mysql_root_pw
echo "your_wp_admin_password" > secrets/wp_admin_pw
echo "your_wp_user_password" > secrets/wp_user_pw
```

Create & fill in `srcs/.env` with your domain and usernames (no passwords in this file). For example:
mkdir -p srcs/.env

```
# Domain
DOMAIN_NAME=jwardeng.42.fr

# MariaDB
MYSQL_DATABASE=wordpress_db
MYSQL_USER=wp_user
MYSQL_HOST=mariadb_in

# WordPress
WP_ADMIN_USER=MegaKatze
WP_ADMIN_EMAIL=Katze@example.com
WP_USER=NormalKatze
WP_USER_EMAIL=NormalKatze@example.com
```

## Starting and stopping the project

Start all services:

```
make up
```

Stop all services:

```
make down
```

---

## Accessing the website and admin panel

**Before accessing the site**, make sure your hosts file is configured correctly. Add the following line to your hosts file:

127.0.0.1 [jwardeng.42.fr](http://jwardeng.42.fr/)

**Linux / macOS:** edit `/etc/hosts` with `sudo nano /etc/hosts`

**Windows:** open `C:\\\\Windows\\\\System32\\\\drivers\\\\etc\\\\hosts` as Administrator

---

### WordPress site

Open your browser and go to:

https://jwardeng.42.fr/

You will see a warning about the self-signed TLS certificate. This is expected.

- **Firefox:** click "Advanced" → "Accept the Risk and Continue"
- **Chrome:** type `thisisunsafe` anywhere on the warning page

---

### WordPress admin panel

https://jwardeng.42.fr/wp-admin

Log in with the WordPress admin credentials stored in `secrets/credentials.txt`.

> The admin username will not contain "admin" or "administrator" — check the credentials file for the exact username.
> 

---

### Adminer (database UI)

[http://localhost:8080](http://localhost:8080/)

On the login screen use:

| Field | Value |
| --- | --- |
| System | MySQL |
| Server | mariadb |
| Username | value from `srcs/.env` → `MYSQL_USER` |
| Password | value from `secrets/db_password.txt` |
| Database | value from `srcs/.env` → `MYSQL_DATABASE` |

---

## Locating and managing credentials

All sensitive values are stored in the `secrets/` folder at the root of the repository. This folder is never committed to Git.

| File | Contains |
| --- | --- |
| `secrets/wp_admin_pw
secrets/wp_user_pw` | WordPress admin & user password |
| `secrets/mysql_pw` | MariaDB user password |
| `secrets/mysql_root_pw`  | MariaDB root password |

Non-sensitive configuration (domain name, database name, usernames) is stored in `srcs/.env`. Open it with any text editor to review or change these values.

---

## Checking that services are running correctly

List all running containers and their status:

```
docker ps
```

All services should show status `Up`. If any show `Exit` or `Restarting`, check their logs.

View logs for a specific service:

```
make nginx-logs
make wp-logs
make db-logs
make rd-logs
make st-logs
make cs-logs
```

Or view all logs at once:

```
docker compose -f srcs/docker-compose.yml logs
```

A quick functional check:

- [https://jwardeng.42.fr](https://jwardeng.42.fr/) loads → NGINX + WordPress are working
- [http://localhost:8080](http://localhost:8080/) loads Adminer → PHP + MariaDB connection is working
- http://localhost/static/ loads the static page → static server is working
- [http://localhost:9090](http://localhost:9090/) & [http://localhost:9110](http://localhost:9110/) loads CatSurf + static pages w. upload & download function