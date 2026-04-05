
init:
	mkdir -p /home/login/data/mariadb /home/login/data/wordpress

COMPOSE=docker compose -f srcs/docker-compose.yml

db-up:
	$(COMPOSE) up --build -d mariadb_in
db-down:
	$(COMPOSE) down -v mariadb_in
db-logs:
	$(COMPOSE) logs -f mariadb_in
db-shell:
	$(COMPOSE) exec -it mariadb_in sh

wp-up:
	$(COMPOSE) up --build -d wordpress_in
wp-down:
	$(COMPOSE) down -v wordpress_in
wp-logs:
	$(COMPOSE) logs -f wordpress_in
wp-shell:
	$(COMPOSE) exec -it wordpress_in sh

nginx-up:
	$(COMPOSE) up -d nginx_in
nginx-down:
	$(COMPOSE) down nginx_in
nginx-logs:
	$(COMPOSE) logs -f nginx_in
nginx-shell:
	$(COMPOSE) exec -it nginx_in sh

up:
	$(COMPOSE) up --build -d
down:
	$(COMPOSE) down -v
logs:
	$(COMPOSE) logs -f

re: down up