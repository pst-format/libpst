if PLATFORM_WIN32
    NO_UNDEFINED = -no-undefined
else
    NO_UNDEFINED =
endif

lib_LTLIBRARIES     = _libpst.la
_libpst_la_LDFLAGS  = -module $(NO_UNDEFINED) -version-info @PYTHON_LIBPST_VERSION_INFO@
_libpst_la_SOURCES  = python-libpst.cpp
_libpst_la_LIBADD   = -lboost_python ../src/libpst.la

# set the include path found by configure
INCLUDES= -I$(srcdir)/.. -I$(srcdir)/../src $(all_includes) -I$(PYTHON_INCLUDE_DIR)
