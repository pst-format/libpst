if PYTHON_INTERFACE

if PLATFORM_WIN32
    NO_UNDEFINED = -no-undefined
else
    NO_UNDEFINED =
endif

pythonlibdir            = $(PYTHON_SITE_PKG)
pythonlib_LTLIBRARIES   = _libpst.la
_libpst_la_LDFLAGS      = -module $(NO_UNDEFINED) -avoid-version
_libpst_la_SOURCES      = python-libpst.cpp
_libpst_la_LIBADD       = @BOOST_PYTHON_LIB@ ../src/libpst.la

# set the include path found by configure
INCLUDES= -I$(srcdir)/.. -I$(srcdir)/../src $(all_includes) -I$(PYTHON_INCLUDE_DIR)

endif
