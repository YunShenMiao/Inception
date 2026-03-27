COMPOSE=docker compose

db-up:
	$(COMPOSE) up --build -d mariadb_in
db-down:
	$(COMPOSE) down -v mariadb_in
db-logs:
	$(COMPOSE) logs -f mariadb_in

wp-up:
	$(COMPOSE) up --build -d wp
wp-down:
	$(COMPOSE) down -v wp
wp-logs:
	$(COMPOSE) logs -f wp

nginx-up:
	$(COMPOSE) up -d nginx
nginx-down:
	$(COMPOSE) down nginx
nginx-logs:
	$(COMPOSE) logs -f nginx

up:
	$(COMPOSE) up -d
down:
	$(COMPOSE) down -v
logs:
	$(COMPOSE) logs -f

restart: down up