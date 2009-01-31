SUBDIRS = src man html info debian libpst
htmldir = ${datadir}/doc/@PACKAGE@-@VERSION@
html_DATA = AUTHORS COPYING ChangeLog NEWS README
CLEANFILES = xml/libpst xml/Makefile

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libpst.pc

EXTRA_DIST = Doxyfile libpst.html.tar.gz libpst.spec $(wildcard xml/M*) $(wildcard xml/h*) $(wildcard xml/lib*)

if !STATIC_TOOLS
    libpstincludedir      = $(includedir)/libpst
    libpstinclude_HEADERS = config.h
endif
