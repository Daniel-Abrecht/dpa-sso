
prefix ?= /usr/local/
MODDIR = /etc/apache2/mods-available/
SOURCES = $(shell find src/ -type f -iname "*.c")

CFLAGS+=$(shell curl-config --cflags)
LDLIBS+=$(shell curl-config --libs)

export prefix
export PREFIX=$(prefix)
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
	mkdir -p "$(DESTDIR)$(prefix)/lib/apache2/modules/"
	cp "bin/mod_auth_dpa-sso.so" "$(DESTDIR)$(prefix)/lib/apache2/modules/mod_auth_dpa-sso.so~"
	mv "$(DESTDIR)$(prefix)/lib/apache2/modules/mod_auth_dpa-sso.so~" "$(DESTDIR)$(prefix)/lib/apache2/modules/mod_auth_dpa-sso.so"
	envsubst <"src/auth_dpa-sso.load.in" >"$(DESTDIR)$(MODDIR)/auth_dpa-sso.load"
