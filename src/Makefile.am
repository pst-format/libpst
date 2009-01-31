if OS_WIN32
    ICONVLIB = -liconv
else
    ICONVLIB =
endif

if STATIC_TOOLS
    PSTLIB=@PST_OBJDIR@/libpst.a
else
    PSTLIB=libpst.la
endif

common_header = common.h        \
                define.h        \
                libpst.h        \
                libstrfunc.h    \
				lzfu.h			\
                timeconv.h      \
                vbuf.h

common_source = debug.c         \
                libpst.c        \
                libstrfunc.c    \
				lzfu.c			\
                timeconv.c      \
                vbuf.c

if NEED_XGETOPT
    common_source += XGetopt.c
endif

noinst_PROGRAMS     = deltasearch dumpblocks getidblock
bin_PROGRAMS        = lspst readpst readpstlog pst2ldif
if BUILD_DII
    bin_PROGRAMS   += pst2dii
endif
lspst_SOURCES       = lspst.c         $(common_header)
readpst_SOURCES     = readpst.c       $(common_header)
readpstlog_SOURCES  = readpstlog.c    $(common_header)
pst2ldif_SOURCES    = pst2ldif.cpp    $(common_header)
pst2dii_SOURCES     = pst2dii.cpp     $(common_header)
deltasearch_SOURCES = deltasearch.cpp $(common_header)
dumpblocks_SOURCES  = dumpblocks.c    $(common_header)
getidblock_SOURCES  = getidblock.c    $(common_header)

lspst_CFLAGS        = $(AM_CFLAGS)
readpst_CFLAGS      = $(AM_CFLAGS)
readpstlog_CFLAGS   = $(AM_CFLAGS)
pst2ldif_CFLAGS     = $(AM_CFLAGS)
pst2dii_CFLAGS      = $(AM_CFLAGS)
deltasearch_CFLAGS  = $(AM_CFLAGS)
dumpblocks_CFLAGS   = $(AM_CFLAGS)
getidblock_CFLAGS   = $(AM_CFLAGS)

if STATIC_TOOLS
    noinst_LTLIBRARIES    = libpst.la
else
    lib_LTLIBRARIES       = libpst.la
    libpstincludedir      = $(includedir)/libpst
    libpstinclude_HEADERS = \
        common.h    \
		define.h	\
        libpst.h    \
        libstrfunc.h\
        timeconv.h  \
        vbuf.h
    libpst_la_LDFLAGS     = -no-undefined -version-info 1:0:0
endif

libpst_la_SOURCES     = $(common_source) $(common_header)
libpst_la_LIBADD      = $(ICONVLIB)


EXTRA_DIST = testdebug.c

# set the include path found by configure
INCLUDES= $(all_includes)

# the library search path.
lspst_LDADD       = $(all_libraries) $(PSTLIB) $(ICONVLIB)
readpst_LDADD     = $(all_libraries) $(PSTLIB) $(ICONVLIB)
readpstlog_LDADD  = $(all_libraries) $(PSTLIB) $(ICONVLIB)
pst2ldif_LDADD    = $(all_libraries) $(PSTLIB) $(ICONVLIB)
pst2dii_LDADD     = $(all_libraries) $(PSTLIB) $(ICONVLIB) -lgd
dumpblocks_LDADD  = $(all_libraries) $(PSTLIB) $(ICONVLIB)
getidblock_LDADD  = $(all_libraries) $(PSTLIB) $(ICONVLIB)
deltasearch_LDADD = $(all_libraries) $(PSTLIB) $(ICONVLIB)
