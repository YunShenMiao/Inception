# Inception

*This project has been created as part of the 42 curriculum by jwardeng.*

## Description

Inception is a multi-container web infrastructure built with Docker Compose. Each service
runs in its own dedicated container, built from custom Dockerfiles using the penultimate stable version of Alpine as the base image. The goal is to understand container orchestration, service isolation, and networking.

### Services

| Service | Description | Access |
| --- | --- | --- |
| NGINX | Reverse proxy, sole entrypoint via TLS | https://localhost * |
| WordPress + php-fpm | Dynamic CMS | https://localhost * (via nginx) |
| MariaDB | Database backend | internal only |
| Redis | WordPress object cache | internal only |
| Static site | HTML/CSS/JS portfolio page | [https://localhost/static/](http://localhost/static/) * |
| Adminer | Database management UI | [http://localhost:8080](http://localhost:8080/) * |
| Webserv (42) | Custom C++ HTTP server with upload/download | [http://localhost:9110](http://localhost:9110/) — 
[http://localhost:9090](http://localhost:9090/) *
(upload/download)  |

*To access the project via `https://jwardeng.42.fr`  or your own domain instead of `localhost`, you need to add an entry to your system's hosts file that points the domain to your local machine. 

When accessing wordpress via https you will see a browser warning about the self-signed TLS certificate — this is expected. Accept the exception and you will reach the WordPress site.

### Design choices

**Virtual Machines vs Docker**
VMs virtualize an entire OS with its own kernel, consuming significant resources. Docker
containers share the host kernel virtualize only the application layer, making them
far lighter and faster to spin up. For this project, containers give us per-service isolation and a standardized environment without the overhead of running six separate VMs.

**Secrets vs Environment Variables**
Environment variables are visible to any process in the container. Docker secrets mount sensitive values as files in `/run/secrets/`, accessible only to the container that needs them. Credentials like database passwords are stored as secrets in this project; non-sensitive config like domain names use `.env`.

**Docker Network vs Host Network**
Host networking shares the host's network stack directly — no isolation, any port the
container opens is immediately exposed. Docker networks create a private virtual network
where containers communicate by service name (e.g. `mariadb`, `wordpress`) and nothing
is exposed to the outside unless explicitly published. This project uses the custom bridge
network ‘Inception’ so containers can reach each other internally while only NGINX is exposed externally on port 443 (for the bonus also CatSurf is exposed externally on port 9110 & 9090).

**Docker Volumes vs Bind Mounts**
Bind mounts link a host path directly into the container — useful for development but
tightly coupled to the host filesystem layout. Named volumes are managed by Docker. The WordPress files and database are stored in named volumes under `/home/youruser/data/` on the host, as required by the subject.

---

## Instructions

### Prerequisites

- Linux: Docker + Docker Compose installed
- Windows / macOS: Docker Desktop

### Setup

Clone the repository and create the required secret files before first run:

```bash
git clone https://github.com/YunShenMiao/Inception.git
cd Inception
```

Create your secrets (never commit these):

mkdir -p secrets
echo "your_db_password" > mysql_pw
echo "your_db_root_password" > secrets/mysql_root_pw
echo "your_wp_admin_password" > secrets/wp_admin_pw
echo "your_wp_user_password" > secrets/wp_user_pw

Fill in `srcs/.env` with your domain and usernames (no passwords in this file). For example:

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

Create volume directories, eg:

```bash
mkdir ../data/mariadb ../data/wordpress
```

Configure your local domain

---

**Linux / macOS**

Open the hosts file with root privileges:

```bash
sudo nano /etc/hosts
```

**Windows**

Open Notepad as Administrator (right-click → "Run as administrator"), then open the file:

C:\Windows\System32\drivers\etc\hosts

Add the following line at the bottom: 127.0.0.1 [jwardeng.42.fr](http://jwardeng.42.fr) (optional: use your own domain)

### Running the project

Build all images and start every service:

```bash
make up
```

Stop and remove all containers:

```bash
make down
```

### Per-service commands

Each service has its own set of targets using a short abbreviation:
`db` (MariaDB), `nginx`, `wp` (WordPress), `rd` (Redis), `st` (static), `cs` (catsurf/webserv)

```bash
make <service>-up      # start a single service
make <service>-down    # stop a single service
make <service>-logs    # tail the container logs
make <service>-shell   # open a shell inside the container
```

Example:

```bash
make db-up
make wp-logs
make nginx-shell
```

---

## Resources

```markdown
DOCKER
    - https://learn.kodekloud.com/courses/docker-training-course-for-the-absolute-beginner
    - https://docs.docker.com/
    - https://docs.docker.com/reference/compose-file
    - https://docs.docker.com/reference/dockerfile/

Wordpress
	 - https://developer.wordpress.org/advanced-administration/before-install/howto-install/
	 - https://wiki.alpinelinux.org/wiki/WordPress
	 - https://developer.wordpress.org/apis/wp-config-php/
	 - https://wp-cli.org/

MariaDB
    - https://www.tutorialspoint.com/mariadb/index.htm
    - https://wiki.alpinelinux.org/wiki/MariaDB
    - https://mariadb.com/docs/server/mariadb-quickstart-guides/mariadb-sql-cheat-cheat-guide
    - https://mariadb.com/docs/server/server-management/install-and-upgrade-mariadb/configuring-mariadb/configuring-mariadb-with-option-files

NGINX
    - https://docs.nginx.com/nginx/admin-guide/web-server/web-server/
    - https://nginx.org/en/docs/http/configuring_https_servers.html

Redis
    - https://redis.io/docs/latest/develop/
    - https://www.youtube.com/watch?v=a4yX7RUgTxI

Adminer
    - https://www.adminer.org/
    - https://www.youtube.com/watch?v=wiIgSVBCY4I
```

### AI usage

Claude was used during this project for the following:

- Reviewing & debugging
- Discussing certain concepts within System Administration & Cloud Engineering
- Improving html files for static websites