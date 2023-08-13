root := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

all: debpkg

define \n


endef
\r=$(shell printf "\r")
$(eval $(subst ${\r},${\n},$(shell $(root)/shenv | tr "\n" '\r')))

FORMAT ?= xz
Z ?= xz
PKG_ORIG    = build/$(NAME)_$(VERSION).orig.tar.$(FORMAT)
PKG_DSC     = build/$(NAME)_$(VERSION)-$(REVISION).dsc
PKG_DEB_SRC = build/$(NAME)_$(VERSION)-$(REVISION).debian.tar.$(FORMAT)

debpkg: bin/.done
archive: $(PKG_ORIG)
package: $(PKG_DSC)

.DELETE_ON_ERROR:

bin/.done: $(PKG_DSC)
	rm -rf bin
	mkdir -p bin
	set -ex; \
	cd bin/; \
	dpkg-source -x "../$(PKG_DSC)"; \
	( cd "$(NAME)-$(VERSION)/" && debuild; ); \
	rm -rf "$(NAME)-$(VERSION)/"
	touch "$@"

$(PKG_DSC): $(PKG_ORIG)
	set -ex; \
	cleanup(){ rm -rf "$$tmp"; }; \
	tmp="$$(realpath "$$(mktemp -d -p build/)")"; \
	trap cleanup EXIT TERM INT; \
	( \
	  cd "$$tmp"; \
	  $(Z) -d <"../../$(PKG_ORIG)" | tar x; \
	  cp -r ../../debian .; \
	  dpkg-source --compression=$(Z) -b . \
	)

$(PKG_ORIG):
	mkdir -p $(dir $@)
	( \
	  cd '$(root)'; \
	  git archive --format=tar '$(COMMIT)'; \
	) | $(Z) > "$@"

clean:
	rm -rf bin/
	rm -f $(PKG_ORIG) $(PKG_DSC) $(PKG_DEB_SRC) $(PKG_DEB)
