# Developer Documentation — Inception

## Prerequisites

Make sure the following are installed on your machine before getting started:

- Docker (v24 or later recommended)
- Docker Compose v2 (included with Docker Desktop, or install the plugin on Linux)
- make
- git

Verify your installation:

```bash
docker --version
docker compose version
make --version
```

---

## Setting up the environment from scratch

### 1. Clone the repository

```bash
git clone https://github.com/YunShenMiao/Inception.git
cd inception
```

### 2. Create the secrets directory and files

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

Use strong passwords. These values are referenced in `srcs/docker-compose.yml` via the
`secrets:` block and read at runtime from `/run/secrets/<filename>` inside containers.

### 3. Configure the environment file

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

### 4. Configure your hosts file

Add this line to `/etc/hosts` (Linux/macOS) or
`C:\\Windows\\System32\\drivers\\etc\\hosts` (Windows):

127.0.0.1 jwardeng.42.fr

or user your own domain and configure in .env

### 5. Service configuration reference

Each service follows the same Dockerfile pattern: `FROM alpine:3.22`, copy config/scripts, `RUN` to install packages, `EXPOSE` the service port, and `ENTRYPOINT` / `CMD` to start the process.

### NGINX (`requirements/nginx/`)

- Built on Alpine, installs `nginx` and `openssl`. A self-signed TLS certificate is generated at build time for the configured domain.
- `nginx.conf` configures a single server on port **443** with TLSv1.2/1.3. PHP requests are forwarded to `wordpress_in:9000` via FastCGI, and `/static/` is reverse-proxied to `static_in:8081`.

### MariaDB (`requirements/mariadb/`)

- Built on Alpine, installs `mariadb` and `mariadb-client`.
- `my.cnf` sets the data directory, binds to `0.0.0.0` (reachable by other containers)
- `init.sh` is the entrypoint — it initializes the database and creates the WordPress user on first boot, then hands off to `mariadbd`.
- Credentials are injected at runtime from `.env` and Docker secrets under `/run/secrets/`.

### WordPress / PHP (`requirements/wordpress/`)

- Built on Alpine, installs PHP 8.3 with FPM and required extensions, plus WP-CLI.
- PHP-FPM listens on port **9000** and handles `.php` requests from NGINX.
- `init.sh` is the entrypoint — on first boot it waits for MariaDB, generates `wp-config.php` (with DB credentials, Redis config, and salts), runs `wp core install`, and creates the admin and subscriber users. It also installs and enables the `redis-cache` plugin on every start.

---

## Building and launching the project

Build all images and start all containers in detached mode:

```bash
make up
```

This internally runs:

```bash
docker compose -f srcs/docker-compose.yml up --build -d
```

Stop and remove all containers (volumes are preserved):

```bash
make down
```

---

## Managing containers and volumes

### Per-service Makefile targets

Each service exposes four targets using its abbreviation:

| Abbreviation | Service |
| --- | --- |
| `db` | MariaDB |
| `nginx` | NGINX |
| `wp` | WordPress |
| `rd` | Redis |
| `st` | Static site |
| `cs` | Webserv (catsurf) |

```bash
make <service>-up      # start the container
make <service>-down    # stop the container
make <service>-logs    # container logs
make <service>-shell   # open an interactive shell inside the container
```

### Volume management

List all Docker volumes:

```bash
docker volume ls
```

Inspect a volume to see its mount point and configuration:

```bash
docker volume inspect <volume-name>
```

---

## Where data is stored and how it persists

This project uses two Docker named volumes for persistent storage:

| Volume | Host path | Contains |
| --- | --- | --- |
| `wordpress_data` | `/home/<user>/data/wordpress` | WordPress core files, themes, uploads |
| `db_data` | `/home/<user>/data/mariadb` | MariaDB database files |

These volumes are defined in `srcs/docker-compose.yml` under the `volumes:` block with a custom `driver_opts` pointing to the host paths above.

Because they are named volumes, they persist across `make down` and `make up` cycles. The custom `driver_opts` in the compose file
makes it possible to store the data locally under `/home/jwardeng/data/` while still using named volumes. Containers can be destroyed and recreated without losing data, even when using down -v.