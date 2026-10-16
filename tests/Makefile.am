# readpst regression tests, run by "make check".
#
# test-embedded-rfc822 builds its input in memory and always runs.
#
# readpst-embedded-rfc822.sh needs a real PST file, and none is shipped.
# Point LIBPST_TEST_PST at a .pst file or a directory of .pst files; without
# it that test is skipped:
#
#   make check LIBPST_TEST_PST=/path/to/mail.pst
#
# Memory errors do not always show in the output, so these tests are most
# useful under AddressSanitizer (or with valgrind installed, which the PST
# test uses automatically on a non-ASan build). For an ASan build:
#
#   export ASAN_OPTIONS=detect_leaks=0
#   ./configure CFLAGS='-g -O1 -fno-omit-frame-pointer -fsanitize=address' \
#               LDFLAGS='-fsanitize=address'
#   make && make check
#
# (detect_leaks=0 is needed for configure too: the leak checker would make its
# iconv test program fail, and the build then fails for lack of iconv.)
#
# An ASan report makes a test fail. On kernels with high mmap randomisation,
# older ASan runtimes spin printing "AddressSanitizer:DEADLYSIGNAL"; run
# configure and "make check" under "setarch $(uname -m) -R" then.

check_PROGRAMS = test-embedded-rfc822
test_embedded_rfc822_SOURCES  = test-embedded-rfc822.c
test_embedded_rfc822_CPPFLAGS = -I$(top_srcdir)/src -I$(top_builddir)
test_embedded_rfc822_LDADD    = $(top_builddir)/src/libpst.la $(LTLIBICONV) $(REGEXLIB) @ZLIB_LIBS@ $(SEM_LIBS)

TESTS = test-embedded-rfc822 readpst-embedded-rfc822.sh
EXTRA_DIST = readpst-embedded-rfc822.sh valgrind.supp
AM_TESTS_ENVIRONMENT = \
    READPST='$(abs_top_builddir)/src/readpst'; export READPST; \
    LIBTOOL='$(LIBTOOL)'; export LIBTOOL; \
    VALGRIND_SUPP='$(abs_srcdir)/valgrind.supp'; export VALGRIND_SUPP; \
    ASAN_OPTIONS=$${ASAN_OPTIONS:-detect_leaks=0}; export ASAN_OPTIONS;
