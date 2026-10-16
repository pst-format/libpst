# readpst regression tests.
#
# They need a real PST file, and none is shipped. Point LIBPST_TEST_PST at a
# .pst file or a directory of .pst files; without it the tests are skipped:
#
#   make check LIBPST_TEST_PST=/path/to/mail.pst
#
# See the comment at the top of each test for what it checks.

TESTS = readpst-parallel-consistency.sh readpst-unreadable-item.sh
EXTRA_DIST = $(TESTS)
AM_TESTS_ENVIRONMENT = \
    READPST='$(abs_top_builddir)/src/readpst'; export READPST;
