PREFIX ?= /usr
DESTDIR ?=
BUILDDIR ?= build
DISTDIR ?= dist
SOURCE_DATE_EPOCH ?= 1786320000

VERSION := 0.5.0
ARCHIVE := $(DISTDIR)/nekowall-$(VERSION).tar.gz
SOURCES := CMakeLists.txt LICENSE Makefile README.md data src

.PHONY: check build install dist clean

build:
	cmake -S . -B "$(BUILDDIR)" -DCMAKE_BUILD_TYPE=Release
	cmake --build "$(BUILDDIR)"

check: build
	"$(BUILDDIR)/nekowall" --version
	# The window is not opened here: a build machine has no session, and the
	# one thing worth checking without one is that the program still runs.

install: build
	DESTDIR="$(DESTDIR)" cmake --install "$(BUILDDIR)" --prefix "$(PREFIX)"

dist:
	mkdir -p "$(DISTDIR)"
	tar --sort=name --owner=0 --group=0 --numeric-owner \
		--mtime="@$(SOURCE_DATE_EPOCH)" \
		--pax-option=delete=atime,delete=ctime \
		--transform='s,^,nekowall-$(VERSION)/,' \
		-cf - $(SOURCES) | gzip -n > "$(ARCHIVE)"
	sha256sum "$(ARCHIVE)"

clean:
	rm -rf "$(BUILDDIR)" "$(DISTDIR)"
