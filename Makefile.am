SUBDIRS = src man html info debian src/libpst
htmldir = ${datadir}/doc/@PACKAGE@-@VERSION@
html_DATA = AUTHORS COPYING ChangeLog NEWS README
CLEANFILES = xml/libpst xml/Makefile

EXTRA_DIST = Doxyfile libpst.html.tar.gz libpst.spec $(wildcard xml/M*) $(wildcard xml/h*) $(wildcard xml/lib*)

if !STATIC_TOOLS
    pkgconfigdir   = $(libdir)/pkgconfig
    pkgconfig_DATA = libpst.pc
endif
