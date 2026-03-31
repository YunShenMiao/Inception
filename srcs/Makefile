

# all:
#	mkdir -p /home/${USER}/data/mariadb
#	mkdir -p /home/${USER}/data/wordpress
#	docker compose -f srcs/docker-compose.yml up --build


COMPOSE=docker compose

db-up:
	$(COMPOSE) up --build -d mariadb_in
db-down:
	$(COMPOSE) down -v mariadb_in
db-logs:
	$(COMPOSE) logs -f mariadb_in

wp-up:
	$(COMPOSE) up --build -d wordpress_in
wp-down:
	$(COMPOSE) down -v wordpress_in
wp-logs:
	$(COMPOSE) logs -f wordpress_in

nginx-up:
	$(COMPOSE) up -d nginx_in
nginx-down:
	$(COMPOSE) down nginx_in
nginx-logs:
	$(COMPOSE) logs -f nginx_in

up:
	$(COMPOSE) up -d
down:
	$(COMPOSE) down -v
logs:
	$(COMPOSE) logs -f

restart: down up