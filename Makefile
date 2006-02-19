#!/usr/bin/make -f

CFLAGS  ?= -g -Wall
PREFIX ?= /usr/local
INSTALL ?= install

#---------------- Do not modify below this point ------------------

INSTALL_DIR     := $(INSTALL) -p -d -o root -g root -m 0755
INSTALL_FILE    := $(INSTALL) -p    -o root -g root -m 0644
INSTALL_PROGRAM := $(INSTALL) -p    -o root -g root -m 0755 # -s
INSTALL_SCRIPT  := $(INSTALL) -p    -o root -g root -m 0755

VERSION = $(shell cat VERSION)
CFLAGS += -DVERSION=\"$(VERSION)\"

DOCS := AUTHORS ChangeLog CREDITS FILE-FORMAT FILE-FORMAT.html LICENSE TODO \
	VERSION

DISTFILES := $(DOCS) Makefile setup1.vdproj XGetopt.c XGetopt.h common.h \
	debug.c define.h dumpblocks.c getidblock.c libpst.c libpst.h \
	libstrfunc.c libstrfunc.h lspst.c lzfu.c lzfu.h moz-script \
	readlog.vcproj readpst.1 readpst.c readpstlog.1 readpstlog.c \
	testdebug.c timeconv.c timeconv.h w32pst.sln w32pst.vcproj \
	nick2ldif.cpp pst2ldif.cpp

PROGS := lspst readpst readpstlog pst2ldif nick2ldif
ALL_PROGS := $(PROGS) dumpblocks getidblock testdebug

all: $(PROGS)

XGetopt.o: XGetopt.h
debug.o: define.h
dumpblocks.o: define.h
getidblock.o: XGetopt.h define.h libpst.h
libpst.o: define.h libstrfunc.h libpst.h timeconv.h
libstrfunc.o: libstrfunc.h
lspst.o: libpst.h timeconv.h
lzfu.o: define.h libpst.h lzfu.h
readpst.o: XGetopt.h libstrfunc.h define.h libpst.h common.h timeconv.h lzfu.h
pst2ldif.o: XGetopt.h libstrfunc.h define.h libpst.h common.h timeconv.h lzfu.h
nick2ldif.o: XGetopt.h libstrfunc.h define.h libpst.h common.h timeconv.h lzfu.h
readpstlog.o: XGetopt.h define.h
testdebug.o: define.h
timeconv.o: timeconv.h common.h

readpst: readpst.o libpst.o timeconv.o libstrfunc.o debug.o lzfu.o
lspst: debug.o libpst.o libstrfunc.o lspst.o timeconv.o
getidblock: getidblock.o libpst.o debug.o libstrfunc.o
testdebug: testdebug.o debug.o
readpstlog: readpstlog.o debug.o
dumpblocks: dumpblocks.o libpst.o debug.o libstrfunc.o

pst2ldif: pst2ldif.o libpst.o timeconv.o libstrfunc.o debug.o lzfu.o
	g++ ${CFLAGS} pst2ldif.cpp -o pst2ldif libpst.o timeconv.o libstrfunc.o debug.o lzfu.o

nick2ldif: nick2ldif.o libpst.o timeconv.o libstrfunc.o debug.o lzfu.o
	g++ ${CFLAGS} nick2ldif.cpp -o nick2ldif libpst.o timeconv.o libstrfunc.o debug.o lzfu.o

clean:
	rm -f core *.o readpst.log $(ALL_PROGS) *~ MANIFEST

distclean: clean
	rm -f libpst-*.tar.gz

install: all
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/bin
	$(INSTALL_PROGRAM) readpst{,log} $(DESTDIR)$(PREFIX)/bin
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/share/man/man1
	$(INSTALL_FILE) readpst{,log}.1 $(DESTDIR)$(PREFIX)/share/man/man1/
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/share/doc/libpst
	$(INSTALL_FILE) $(DOCS) $(DESTDIR)$(PREFIX)/share/doc/libpst/

uninstall:
	-rm -f $(DESTDIR)$(PREFIX)/bin/readpst{,log}
	-rm -f $(DESTDIR)$(PREFIX)/share/man/man1/readpst{,log}.1

# stolen from ESR's Software Release Practices HOWTO available at:
# http://en.tldp.org/HOWTO/Software-Release-Practice-HOWTO/distpractice.html
MANIFEST: Makefile
	@ls $(DISTFILES) | sed s:^:libpst-$(VERSION)/: >MANIFEST
tarball libpst-$(VERSION).tar.gz: MANIFEST $(DISTFILES)
	@(cd ..; ln -s libpst libpst-$(VERSION))
	(cd ..; tar -czvf libpst/libpst-$(VERSION).tar.gz `cat libpst/MANIFEST`)
	@(cd ..; rm libpst-$(VERSION))
	@rm -f MANIFEST

.PHONY: clean distclean uninstall install tarball
