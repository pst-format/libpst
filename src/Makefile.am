hfiles = $(wildcard *.h)
bin_PROGRAMS        = lspst readpst readpstlog pst2ldif
if BUILD_DII
    bin_PROGRAMS   += pst2dii
endif
noinst_PROGRAMS     = deltasearch dumpblocks getidblock

lspst_SOURCES       = $(hfiles) lspst.c      libpst.c timeconv.c libstrfunc.c debug.c        vbuf.c
readpst_SOURCES     = $(hfiles) readpst.c    libpst.c timeconv.c libstrfunc.c debug.c lzfu.c vbuf.c
readpstlog_SOURCES  = $(hfiles) readpstlog.c                                  debug.c
pst2ldif_SOURCES    = $(hfiles) pst2ldif.cpp libpst.c timeconv.c libstrfunc.c debug.c lzfu.c vbuf.c
pst2dii_SOURCES     = $(hfiles) pst2dii.cpp  libpst.c timeconv.c libstrfunc.c debug.c lzfu.c vbuf.c
deltasearch_SOURCES = deltasearch.cpp debug.c
dumpblocks_SOURCES  = dumpblocks.c    debug.c libpst.c libstrfunc.c timeconv.c vbuf.c
getidblock_SOURCES  = getidblock.c    debug.c libpst.c libstrfunc.c timeconv.c vbuf.c

if BUILD_LIBPST_SHARED
    lspst_CFLAGS        = $(AM_CFLAGS)
    readpst_CFLAGS      = $(AM_CFLAGS)
    readpstlog_CFLAGS   = $(AM_CFLAGS)
    pst2ldif_CFLAGS     = $(AM_CFLAGS)
    pst2dii_CFLAGS      = $(AM_CFLAGS)
    deltasearch_CFLAGS  = $(AM_CFLAGS)
    dumpblocks_CFLAGS   = $(AM_CFLAGS)
    getidblock_CFLAGS   = $(AM_CFLAGS)
    lib_LTLIBRARIES = libpst.la
    libpst_la_SOURCES = common.h        \
                        debug.c 		\
                        libpst.c        \
                        libpst.h        \
                        libstrfunc.c    \
                        libstrfunc.h    \
                        lspst.c         \
                        timeconv.c      \
                        timeconv.h      \
                        vbuf.c          \
                        vbuf.h          \
                        version.h
    libpst_la_LDFLAGS = -no-undefined
    libpstincludedir = $(includedir)/libpst
    libpstinclude_HEADERS = common.h        \
                            define.h        \
                            libpst.h        \
                            libstrfunc.h	\
                            timeconv.h      \
                            vbuf.h          \
                            version.h
endif

if NEED_XGETOPT
    lspst_SOURCES       += XGetopt.c
    readpst_SOURCES     += XGetopt.c
    readpstlog_SOURCES  += XGetopt.c
    pst2ldif_SOURCES    += XGetopt.c
    pst2dii_SOURCES     += XGetopt.c
endif

EXTRA_DIST = testdebug.c

# set the include path found by configure
INCLUDES= $(all_includes)

# the library search path.
lspst_LDFLAGS      = $(all_libraries)
readpst_LDFLAGS    = $(all_libraries)
readpstlog_LDFLAGS = $(all_libraries)
pst2ldif_LDFLAGS   = $(all_libraries)
pst2dii_LDFLAGS    = $(all_libraries) -lgd -lpng -lz -lfreetype -lm

