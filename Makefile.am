ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src python man html
htmldir = ${datadir}/doc/@PACKAGE@-@VERSION@
html_DATA = AUTHORS COPYING ChangeLog NEWS README
CLEANFILES = xml/libpst xml/Makefile

EXTRA_DIST = Doxyfile libpst.html.tar.gz $(wildcard xml/M*) $(wildcard xml/h*) $(wildcard xml/lib*)

if !STATIC_TOOLS
    pkgconfigdir   = $(libdir)/pkgconfig
    pkgconfig_DATA = libpst.pc
endif
