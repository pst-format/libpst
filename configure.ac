AC_PREREQ(2.60)
AC_INIT(libpst,0.6.76,https://github.com/pst-format/libpst/issues)
AC_CONFIG_SRCDIR([src/libpst.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE
AC_CANONICAL_HOST
AC_USE_SYSTEM_EXTENSIONS

#
#  1. Remember that version-info is current:revision:age, and age <= current.
#  2. If the source code has changed at all since the last public release,
#     then increment revision (`c:r:a' becomes `c:r+1:a').
#  3. If any interfaces have been added, removed, or changed since the last
#     update, increment current, and set revision to 0.
#  4. If any interfaces have been added since the last public release, then
#     increment age, since we should be backward compatible with the previous
#     version.
#  5. If any interfaces have been removed or changed since the last public
#     release, then set age to 0, since we are not backward compatible.
#  6. libtool will build libpst.so.x.y.z where the SONAME is libpst.so.x
#     and x=current-age, y=age, z=revision

libpst_version_info='5:18:1'
AC_SUBST(LIBPST_VERSION_INFO, [$libpst_version_info])
libpst_so_major='4'
AC_SUBST(LIBPST_SO_MAJOR, [$libpst_so_major])

# libpst
# version   soname          so library name
# 0.6.35    libpst.so.2     libpst.so.2.0.0
# 0.6.37    libpst.so.2     libpst.so.2.1.0
# 0.6.38    libpst.so.2     libpst.so.2.1.0
# 0.6.40    libpst.so.4     libpst.so.4.0.0
# 0.6.43    libpst.so.4     libpst.so.4.0.1
# 0.6.47    libpst.so.4     libpst.so.4.0.2
# 0.6.48    libpst.so.4     libpst.so.4.0.3
# 0.6.49    libpst.so.4     libpst.so.4.0.4
# 0.6.50    libpst.so.4     libpst.so.4.1.0
# 0.6.51    libpst.so.4     libpst.so.4.1.1
# 0.6.52    libpst.so.4     libpst.so.4.1.2
# 0.6.53    libpst.so.4     libpst.so.4.1.3
# 0.6.54    libpst.so.4     libpst.so.4.1.4
# 0.6.55    libpst.so.4     libpst.so.4.1.5
# 0.6.56    libpst.so.4     libpst.so.4.1.6
# 0.6.57    libpst.so.4     libpst.so.4.1.6
# 0.6.58    libpst.so.4     libpst.so.4.1.7
# 0.6.59    libpst.so.4     libpst.so.4.1.8
# 0.6.60    libpst.so.4     libpst.so.4.1.9
# 0.6.61    libpst.so.4     libpst.so.4.1.9
# 0.6.62    libpst.so.4     libpst.so.4.1.9
# 0.6.63    libpst.so.4     libpst.so.4.1.10
# 0.6.66    libpst.so.4     libpst.so.4.1.11
# 0.6.67    libpst.so.4     libpst.so.4.1.12
# 0.6.68    libpst.so.4     libpst.so.4.1.13
# 0.6.69    libpst.so.4     libpst.so.4.1.14
# 0.6.73    libpst.so.4     libpst.so.4.1.15
# 0.6.74    libpst.so.4     libpst.so.4.1.16
# 0.6.75    libpst.so.4     libpst.so.4.1.17
# 0.6.76    libpst.so.4     libpst.so.4.1.18



# Check for solaris
AC_MSG_CHECKING([for Solaris])
case "$host" in
    *solaris*)
        os_solaris=yes
        ;;
    *)
        os_solaris=no
        ;;
esac
AC_MSG_RESULT($os_solaris)
AM_CONDITIONAL(OS_SOLARIS, [test "$os_solaris" = "yes"])


# Check for win32
AC_MSG_CHECKING([for Win32])
case "$host" in
    *-mingw*)
        os_win32=yes
        ;;
    *)
        os_win32=no
        ;;
esac
AC_MSG_RESULT($os_win32)
AM_CONDITIONAL(OS_WIN32, [test "$os_win32" = "yes"])


# Check for Win32 platform
AC_MSG_CHECKING([for Win32 platform in general])
case "$host" in
    *-cygwin*)
        platform_win32=yes
        ;;
    *)
        platform_win32=$os_win32
        ;;
esac
AC_MSG_RESULT($platform_win32)
AM_CONDITIONAL(PLATFORM_WIN32, [test "$platform_win32" = "yes"])


# Checks for programs.
# The following lines adds the --enable-dii option to configure:
#
# Give the user the choice to enter one of these:
# --enable-dii
# --enable-dii=yes
# --enable-dii=no
#
AC_MSG_CHECKING([whether we are enabling dii utility])
AC_ARG_ENABLE(dii,
    AC_HELP_STRING([--enable-dii], [enable dii utility]),
    [
        case "${enableval}" in
          yes) ;;
          no)  ;;
          *)   AC_MSG_ERROR(bad value ${enableval} for --enable-dii) ;;
        esac
    ],
    # default if not specified
    enable_dii=yes
    )
AC_MSG_RESULT([$enable_dii])
AC_PATH_PROG(CONVERT, convert)
if test "x$CONVERT" = "x" ; then
    if test "$enable_dii" = "yes"; then
        enable_dii=no
        AC_MSG_WARN([convert program not found. pst2dii disabled])
    fi
else
    if test "x`$CONVERT --version 2>&1 | grep -i imagemagick >/dev/null ; echo $?`" != "x0"; then
        if test "$enable_dii" = "yes"; then
            enable_dii=no
            AC_MSG_WARN([wrong convert program found. pst2dii disabled])
        fi
    fi
fi
AC_CHECK_HEADER([gd.h],
                [
                    AC_DEFINE([HAVE_GD_H], [1], [Define to 1 if you have the <gd.h> header file.])
                ],
                [
                    if test "$enable_dii" = "yes"; then
                        enable_dii=no
                        AC_MSG_WARN([gd.h not found. pst2dii disabled])
                    fi
                ])
AM_CONDITIONAL(BUILD_DII, [test "$enable_dii" = "yes"])


# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET


# make sure we get large file support
AC_SYS_LARGEFILE
AC_CHECK_SIZEOF(off_t)


# Checks for header files.
AC_CHECK_HEADER([unistd.h],
    AM_CONDITIONAL(NEED_XGETOPT, [test yes = no]),
    AM_CONDITIONAL(NEED_XGETOPT, [test yes = yes])
    )
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([ctype.h dirent.h errno.h fcntl.h inttypes.h limits.h regex.h semaphore.h signal.h stdarg.h stdint.h stdio.h stdlib.h string.h sys/param.h sys/shm.h sys/stat.h sys/types.h time.h unistd.h wchar.h])
save_libs="$LIBS" ; LIBS=""
AC_SEARCH_LIBS([sem_init], [pthread rt], [SEM_LIBS="$LIBS"], [AC_MSG_ERROR([sem_init missing])])
AC_SUBST([SEM_LIBS])
LIBS="$save_libs"


# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_HEADER_SYS_WAIT
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_PID_T
AC_STRUCT_TM


# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_FSEEKO
AC_FUNC_STAT
AC_FUNC_LSTAT
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
if test "$cross_compiling" != "yes"; then
    AC_FUNC_MALLOC
    AC_FUNC_MKTIME
    AC_FUNC_REALLOC
fi
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([chdir getcwd memchr memmove memset regcomp strcasecmp strncasecmp strchr strdup strerror strpbrk strrchr strstr strtol get_current_dir_name])
AM_GNU_GETTEXT
AM_GNU_GETTEXT_VERSION([0.17])
AM_ICONV
if test "$am_cv_func_iconv" != "yes"; then
    AC_MSG_ERROR([libpst requires iconv which is missing])
fi


AC_CHECK_FUNCS(regexec,,[AC_CHECK_LIB(regex,regexec,
               [REGEXLIB=-lregex
                AC_DEFINE(HAVE_REGEXEC,1,[Define to 1 if you have the regexec function.])],
               [AC_MSG_ERROR([No regex library found])])])
AC_SUBST(REGEXLIB)


# The following lines adds the --enable-pst-debug option to configure:
#
# Give the user the choice to enter one of these:
# --enable-pst-debug
# --enable-pst-debug=yes
# --enable-pst-debug=no
#
AC_MSG_CHECKING([whether we are forcing debug dump file creation])
AC_ARG_ENABLE(pst-debug,
    AC_HELP_STRING([--enable-pst-debug], [force debug dump file creation]),
    [
        case "${enableval}" in
          yes) ;;
          no)  ;;
          *)   AC_MSG_ERROR(bad value ${enableval} for --enable-pst-debug) ;;
        esac
    ],
    # default if not specified
    enable_pst_debug=no
    )
AC_MSG_RESULT([$enable_pst_debug])
if test "$enable_pst_debug" = "yes"; then
    AC_DEFINE(DEBUG_ALL, 1, Define to 1 to force debug dump file creation)
fi


# The following lines adds the --enable-libpst-shared option to configure:
#
# Give the user the choice to enter one of these:
# --enable-libpst-shared
# --enable-libpst-shared=yes
# --enable-libpst-shared=no
#
AC_MSG_CHECKING([whether we are building libpst shared object])
AC_ARG_ENABLE(libpst-shared,
    AC_HELP_STRING([--enable-libpst-shared], [build libpst shared object]),
    [
        case "${enableval}" in
          yes) ;;
          no)  ;;
          *)   AC_MSG_ERROR(bad value ${enableval} for --enable-libpst-shared) ;;
        esac
    ],
    # default if not specified
    enable_libpst_shared=no
    )
AC_MSG_RESULT([$enable_libpst_shared])
enable_static_tools=yes
if test "$enable_libpst_shared" = "yes"; then
    enable_shared=yes
    enable_static_tools=no
fi

# needed by STATIC_TOOLS in src/Makefile.am
AC_SUBST(PST_OBJDIR, [$objdir])


# The following lines adds the --enable-static-tools option to configure:
#
# Give the user the choice to enter one of these:
# --enable-static-tools
# --enable-static-tools=yes
# --enable-static-tools=no
#
AC_MSG_CHECKING([whether to link command line tools with libpst statically])
AC_ARG_ENABLE([static-tools],
    AC_HELP_STRING([--enable-static-tools], [link command line tools with libpst statically]),
    [
        case "${enableval}" in
          yes) ;;
          no)  ;;
          *)   AC_MSG_ERROR(bad value ${enableval} for --enable-static-tools) ;;
        esac
    ],
    [
        enable_static_tools=no
    ])
AC_MSG_RESULT([$enable_static_tools])
AM_CONDITIONAL(STATIC_TOOLS, [test "$enable_static_tools" = "yes"])
if test "$enable_static_tools" = "yes"; then
	enable_static="yes"
fi


# The following lines adds the --enable-python option to configure:
#
# Give the user the choice to enter one of these:
# --enable-python
# --enable-python=yes
# --enable-python=no
#
AC_MSG_CHECKING([whether to build the libpst python interface])
AC_ARG_ENABLE([python],
    AC_HELP_STRING([--enable-python], [build libpst python interface]),
    [
        case "${enableval}" in
          yes) ;;
          no)  ;;
          *)   AC_MSG_ERROR(bad value ${enableval} for --python) ;;
        esac
    ],
    [
        enable_python=yes
    ])
AC_MSG_RESULT([$enable_python])
AM_CONDITIONAL(PYTHON_INTERFACE, [test "$enable_python" = "yes"])
if test "$enable_python" = "yes"; then
	enable_shared="yes"
    # get the version of installed python
    AX_PYTHON
    if test "$ax_python_bin" = "no"; then
        AC_MSG_ERROR(python binary not found)
    fi
    PYTHON_VERSION=`echo $ax_python_bin | cut -c7-`

    # find the flags for that version
    AX_PYTHON_DEVEL([>= '$PYTHON_VERSION'])

    # do we have Boost Python?
    AX_BOOST_PYTHON
    if test "$ac_cv_boost_python" = "no"; then
        AC_MSG_ERROR(Boost Python not found)
    fi
    AC_SUBST(PYTHON_VERSION, [$ax_python_bin])
fi


# The following lines adds the --enable-profiling option to configure:
#
# Give the user the choice to enter one of these:
# --enable-profiling
# --enable-profiling=yes
# --enable-profiling=no
#
AC_MSG_CHECKING([whether to link with gprof profiling])
AC_ARG_ENABLE([profiling],
    AC_HELP_STRING([--enable-profiling], [link with gprof profiling]),
    [
        case "${enableval}" in
          yes)
              CFLAGS="$CFLAGS -pg"
              CPPFLAGS="$CPPFLAGS -pg"
              CXXFLAGS="$CXXFLAGS -pg"
              ;;
          no)
              ;;
          *)   AC_MSG_ERROR(bad value ${enableval} for --profiling) ;;
        esac
    ],
    [
        enable_profiling=no
    ])
AC_MSG_RESULT([$enable_profiling])
AM_CONDITIONAL(GPROF_PROFILING, [test "$enable_profiling" = "yes"])

PKG_CHECK_MODULES([GSF], [libgsf-1])

PKG_CHECK_MODULES([ZLIB], [zlib])

AC_OUTPUT(                  \
    Makefile                \
    html/Makefile           \
    libpst.pc               \
    libpst.spec             \
    man/Makefile            \
    src/Makefile            \
    src/pst2dii.cpp         \
    python/Makefile         \
    xml/Makefile            \
    xml/libpst              \
    )
