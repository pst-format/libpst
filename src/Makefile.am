hfiles = $(wildcard *.h)
bin_PROGRAMS = lspst readpst readpstlog pst2ldif
lspst_SOURCES      = $(hfiles) debug.c libpst.c libstrfunc.c lspst.c timeconv.c
readpst_SOURCES    = $(hfiles) readpst.c libpst.c timeconv.c libstrfunc.c debug.c lzfu.c
readpstlog_SOURCES = $(hfiles) readpstlog.c debug.c
pst2ldif_SOURCES   = $(hfiles) pst2ldif.cpp libpst.c timeconv.c libstrfunc.c debug.c lzfu.c
EXTRA_DIST = dumpblocks.c getidblock.c testdebug.c

# set the include path found by configure
INCLUDES= $(all_includes)

# the library search path.
lspst_LDFLAGS      = $(all_libraries)
readpst_LDFLAGS    = $(all_libraries)
readpstlog_LDFLAGS = $(all_libraries)
pst2ldif_LDFLAGS   = $(all_libraries)

# default compile flags
lspst_CXXFLAGS      =
readpst_CXXFLAGS    =
readpstlog_CXXFLAGS =
pst2ldif_CXXFLAGS   =
