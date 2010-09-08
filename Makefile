# sysmenu - dynamic menu
# See LICENSE file for copyright and license details.

include config.mk

SRC = sysmenu.c
OBJ = ${SRC:.c=.o}

all: options sysmenu

options:
	@echo sysmenu build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

sysmenu: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f sysmenu ${OBJ} sysmenu-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p sysmenu-${VERSION}
	@cp -R LICENSE Makefile README config.mk sysmenu.1 config.h sysmenu_path sysmenu_run ${SRC} sysmenu-${VERSION}
	@tar -cf sysmenu-${VERSION}.tar sysmenu-${VERSION}
	@gzip sysmenu-${VERSION}.tar
	@rm -rf sysmenu-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f sysmenu sysmenu_path sysmenu_run ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/sysmenu
	@chmod 755 ${DESTDIR}${PREFIX}/bin/sysmenu_path
	@chmod 755 ${DESTDIR}${PREFIX}/bin/sysmenu_run
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < sysmenu.1 > ${DESTDIR}${MANPREFIX}/man1/sysmenu.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/sysmenu.1

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/sysmenu ${DESTDIR}${PREFIX}/bin/sysmenu_path
	@rm -f ${DESTDIR}${PREFIX}/bin/sysmenu ${DESTDIR}${PREFIX}/bin/sysmenu_run
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/sysmenu.1

.PHONY: all options clean dist install uninstall
