SUBDIRS = src man html info debian
htmldir = ${datadir}/doc/@PACKAGE@-@VERSION@
html_DATA = AUTHORS COPYING ChangeLog NEWS README
CLEANFILES = xml/libpst xml/Makefile

if BUILD_LIBPST_SHARED
    pkgconfigdir = $(libdir)/pkgconfig
    pkgconfig_DATA = libpst.pc
endif

EXTRA_DIST = Doxyfile libpst.html.tar.gz libpst.spec $(wildcard xml/M*) $(wildcard xml/h*) $(wildcard xml/lib*)

