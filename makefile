PAM_LIB_DIR ?= /lib/security
INSTALL ?= install
CFLAGS ?= -O2 -Wall -Wextra -pedantic

CFLAGS += -fPIC -fvisibility=hidden -std=c17 -ffunction-sections -fdata-sections
LDFLAGS += -shared -Wl,-gc-sections

LDLIBS = -lpam

CFLAGS+=$(shell curl-config --cflags)
LDLIBS+=$(shell curl-config --libs)

.PRECIOUS:

all: bin/pam_dpa-sso.so

build/%.o: src/%.c
	mkdir -p build/
	$(CC) -c $(CFLAGS) $^ -o $@

bin/%.so: build/%.o
	mkdir -p bin/
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

install:
	$(INSTALL) -m 0755 -d $(DESTDIR)$(PAM_LIB_DIR)
	$(INSTALL) -m 0755 bin/pam_dpa-sso.so $(DESTDIR)$(PAM_LIB_DIR)

clean:
	rm -rf build/ bin/
