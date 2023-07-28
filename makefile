
MODDIR = /etc/apache2/mods-available/
PREFIX = /usr/local/
SOURCES = $(shell find src/ -type f -iname "*.c")

CFLAGS+=$(shell curl-config --cflags)
LDLIBS+=$(shell curl-config --libs)

export PREFIX
export CFLAGS
export LDLIBS

bin/mod_auth_dpa-sso.so: $(SOURCES)
	rm -rf build bin
	mkdir bin build
	cp -r src build/src
	cd build && apxs -o mod_auth_dpa-sso.so -a -c $^ $(CFLAGS) $(LDLIBS)
	mv build/.libs/mod_auth_dpa-sso.so $@

clean:
	rm -rf build bin

install:
	mkdir -p "$(DESTDIR)$(PREFIX)/lib/apache2/modules/"
	cp "bin/mod_auth_dpa-sso.so" "$(DESTDIR)$(PREFIX)/lib/apache2/modules/mod_auth_dpa-sso.so~"
	mv "$(DESTDIR)$(PREFIX)/lib/apache2/modules/mod_auth_dpa-sso.so~" "$(DESTDIR)$(PREFIX)/lib/apache2/modules/mod_auth_dpa-sso.so"
	envsubst <"src/auth_dpa-sso.load.in" >"$(MODDIR)/auth_dpa-sso.load"
