if PLATFORM_WIN32
    NO_UNDEFINED = -no-undefined
else
    NO_UNDEFINED =
endif


if OS_SOLARIS
    AM_CFLAGS = -D_POSIX_PTHREAD_SEMANTICS
endif



if STATIC_TOOLS
    PSTLIB = @PST_OBJDIR@/libpst.a
else
    PSTLIB = libpst.la
endif


common_header = common.h        \
                define.h        \
                libpst.h        \
                libstrfunc.h    \
                lzfu.h          \
                timeconv.h      \
                vbuf.h

common_source = debug.c         \
                libpst.c        \
                libstrfunc.c    \
                lzfu.c          \
                timeconv.c      \
                vbuf.c


if NEED_XGETOPT
    common_source += XGetopt.c
    common_header += XGetopt.h
endif

noinst_PROGRAMS     = deltasearch dumpblocks getidblock
bin_PROGRAMS        = lspst readpst pst2ldif nick2ldif
if BUILD_DII
    bin_PROGRAMS   += pst2dii
endif
lspst_SOURCES       = lspst.c          $(common_header)
readpst_SOURCES     = readpst.c        $(common_header) msg.cpp msg.h
pst2ldif_SOURCES    = pst2ldif.cpp     $(common_header)
pst2dii_SOURCES     = pst2dii.cpp      $(common_header)
deltasearch_SOURCES = deltasearch.cpp  $(common_header)
dumpblocks_SOURCES  = dumpblocks.c     $(common_header)
getidblock_SOURCES  = getidblock.c     $(common_header)
nick2ldif_SOURCES   = nick2ldif.cpp    $(common_header)

readpst_CPPFLAGS    = $(AM_CPPFLAGS) $(GSF_CFLAGS)

lspst_DEPENDENCIES        = libpst.la
readpst_DEPENDENCIES      = libpst.la
pst2ldif_DEPENDENCIES     = libpst.la
pst2dii_DEPENDENCIES      = libpst.la
deltasearch_DEPENDENCIES  = libpst.la
dumpblocks_DEPENDENCIES   = libpst.la
getidblock_DEPENDENCIES   = libpst.la
nick2ldif_DEPENDENCIES    = libpst.la

if STATIC_TOOLS
    noinst_LTLIBRARIES    = libpst.la
else
    lib_LTLIBRARIES       = libpst.la
    libpstincludedir      = $(includedir)/libpst-@LIBPST_SO_MAJOR@/libpst
    libpstinclude_HEADERS = \
        common.h    \
        libpst.h    \
        libstrfunc.h\
        lzfu.h      \
        timeconv.h  \
        vbuf.h
    libpst_la_LDFLAGS     = $(NO_UNDEFINED) -version-info @LIBPST_VERSION_INFO@
endif

libpst_la_SOURCES     = $(common_source) $(common_header)
libpst_la_LIBADD      = $(LTLIBICONV)

EXTRA_DIST =
if !NEED_XGETOPT
    EXTRA_DIST += XGetopt.c XGetopt.h
endif

# set the include path found by configure
INCLUDES= -I$(srcdir)/.. $(all_includes)

# the library search path.
lspst_LDADD       = $(all_libraries) $(PSTLIB) $(LTLIBICONV) @ZLIB_LIBS@
readpst_LDADD     = $(all_libraries) $(PSTLIB) $(LTLIBICONV) $(REGEXLIB) $(GSF_LIBS) @ZLIB_LIBS@
pst2ldif_LDADD    = $(all_libraries) $(PSTLIB) $(LTLIBICONV) @ZLIB_LIBS@
pst2dii_LDADD     = $(all_libraries) $(PSTLIB) $(LTLIBICONV) -lgd @ZLIB_LIBS@
deltasearch_LDADD = $(all_libraries) $(PSTLIB) $(LTLIBICONV) @ZLIB_LIBS@
dumpblocks_LDADD  = $(all_libraries) $(PSTLIB) $(LTLIBICONV) @ZLIB_LIBS@
getidblock_LDADD  = $(all_libraries) $(PSTLIB) $(LTLIBICONV) @ZLIB_LIBS@
nick2ldif_LDADD   = $(all_libraries) $(PSTLIB) $(LTLIBICONV) @ZLIB_LIBS@


