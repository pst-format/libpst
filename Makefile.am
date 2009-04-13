SUBDIRS = src man html debian
htmldir = ${datadir}/doc/@PACKAGE@-@VERSION@
html_DATA = AUTHORS COPYING ChangeLog NEWS README
CLEANFILES = xml/libpst xml/Makefile

EXTRA_DIST = config.rpath Doxyfile libpst.html.tar.gz libpst.spec $(wildcard xml/M*) $(wildcard xml/h*) $(wildcard xml/lib*)

if !STATIC_TOOLS
    pkgconfigdir   = $(libdir)/pkgconfig
    pkgconfig_DATA = libpst.pc
endif
