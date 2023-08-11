PREFIX = /usr/local/
APP_DIR = $(PREFIX)/share/dpa-sso-portal/
LIB_DIR = $(APP_DIR)/lib/
WWW_DIR = $(APP_DIR)/www/
CONFIG_FILE = /etc/dpa-sso-portal/config.php

export APP_DIR WWW_DIR LIB_DIR CONFIG_FILE

help:
	@echo "You can install it to $(WWW_DIR) and $(LIB_DIR) using 'sudo make install'"
	@echo "You'll still have to configure the webserver yourself, just set the document root to $(WWW_DIR)"
	@echo "The config file will be at $(CONFIG_FILE) see the readme for more informations"

install:
	mkdir -p "$(DESTDIR)$(APP_DIR)"
	rm -f "$(DESTDIR)$(LIB_DIR)config.php"
	cp -r app/lib/./ "$(DESTDIR)$(LIB_DIR)"
	cp -r app/www/./ "$(DESTDIR)$(WWW_DIR)"
	envsubst <app/config.php.in >"$(DESTDIR)$(WWW_DIR)config.php"
	rm -f "$(DESTDIR)$(LIB_DIR)config.php"
	ln -s "$(CONFIG_FILE)" "$(DESTDIR)$(LIB_DIR)config.php"
	mkdir -p "$(dir $(DESTDIR)$(CONFIG_FILE))"
	[ -f "$(DESTDIR)$(CONFIG_FILE)" ] || cp app/lib/config.php $(DESTDIR)$(CONFIG_FILE)
