#!/bin/sh
# Regression test: readpst must not read freed memory when a message embeds
# more than one message/rfc822 attachment and has no transport headers.
#
# write_normal_email() used to leave extra_mime_headers pointing into an
# embedded message's own headers, which write_embedded_message() frees right
# after. find_rfc822_headers() then read (and wrote) that freed memory for the
# next embedded message. On musl readpst crashed or cut the .eml short; on
# glibc the output usually looked right.
#
# For every PST in $LIBPST_TEST_PST (a .pst file, or a directory of them), the
# test runs "readpst -j 0 -e -8" and fails if:
#   - readpst does not exit 0 (a crash, or an AddressSanitizer report), or
#     prints a sanitizer report on stderr;
#   - valgrind is installed and reports a memory error (valgrind is not used
#     on an ASan build; set LIBPST_TEST_VALGRIND=no to turn it off);
#   - an .eml file opens a readpst multipart boundary that it never closes,
#     which is what a message cut short looks like.
#
# Without valgrind or ASan, a glibc build usually passes even with the bug, so
# run it under one of them. AddressSanitizer build (see tests/Makefile.am):
#   export ASAN_OPTIONS=detect_leaks=0
#   ./configure CFLAGS='-g -O1 -fno-omit-frame-pointer -fsanitize=address' \
#               LDFLAGS='-fsanitize=address'
#   make check LIBPST_TEST_PST=/path/to/mail.pst
# On kernels with high mmap randomisation, older ASan runtimes spin printing
# "AddressSanitizer:DEADLYSIGNAL"; run under "setarch $(uname -m) -R" then.
#
# Run directly: READPST=src/readpst LIBPST_TEST_PST=... tests/readpst-embedded-rfc822.sh
# Exit status 77 means skipped (no PST given), as automake expects.

READPST=${READPST:-src/readpst}
LIBPST_TEST_VALGRIND=${LIBPST_TEST_VALGRIND:-auto}
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0}
export ASAN_OPTIONS

if [ -z "$LIBPST_TEST_PST" ]; then
    echo "SKIP: set LIBPST_TEST_PST to a .pst file or a directory of .pst files"
    exit 77
fi
if [ -d "$LIBPST_TEST_PST" ]; then
    psts=$(find "$LIBPST_TEST_PST" -type f -name '*.pst' | sort)
else
    psts=$LIBPST_TEST_PST
fi
if [ -z "$psts" ]; then
    echo "SKIP: no .pst files in $LIBPST_TEST_PST"
    exit 77
fi

# readpst may be a libtool wrapper script; valgrind must run the real binary
run_readpst() {
    if [ -n "$valgrind" ]; then
        if [ -n "$LIBTOOL" ] && head -c 2 "$READPST" | grep -q '^#!'; then
            $LIBTOOL --mode=execute $valgrind "$READPST" "$@"
        else
            $valgrind "$READPST" "$@"
        fi
    else
        "$READPST" "$@"
    fi
}

valgrind=
if [ "$LIBPST_TEST_VALGRIND" != no ] && command -v valgrind >/dev/null 2>&1; then
    if grep -q __asan_init "$READPST" 2>/dev/null; then
        echo "readpst is an ASan build; not using valgrind"
    else
        valgrind="valgrind -q --error-exitcode=99"
        if [ -n "$VALGRIND_SUPP" ] && [ -f "$VALGRIND_SUPP" ]; then
            valgrind="$valgrind --suppressions=$VALGRIND_SUPP"
        fi
    fi
elif [ "$LIBPST_TEST_VALGRIND" = yes ]; then
    echo "FAIL: LIBPST_TEST_VALGRIND=yes but valgrind is not installed"
    exit 1
fi

work=$(mktemp -d "${TMPDIR:-.}/readpst-test.XXXXXX") || exit 1
trap 'rm -rf "$work"' 0 INT TERM

fail=0
echo "$psts" | while IFS= read -r pst; do
    out=$work/out
    rm -rf "$out"
    mkdir "$out"
    run_readpst -j 0 -e -8 -o "$out" "$pst" >"$work/stdout" 2>"$work/stderr"
    rc=$?
    if [ "$rc" -ne 0 ]; then
        echo "FAIL: $pst: readpst exited $rc"
        cat "$work/stderr"
        exit 1
    fi
    # a sanitizer report fails the test even if readpst exited 0, e.g. with
    # ASAN_OPTIONS=halt_on_error=0 or in a forked child
    if grep -q -e 'ERROR: AddressSanitizer' -e 'runtime error:' "$work/stderr"; then
        echo "FAIL: $pst: sanitizer report"
        cat "$work/stderr"
        exit 1
    fi
    n=0
    find "$out" -type f -name '*.eml' >"$work/emls"
    while IFS= read -r eml; do
        n=$((n+1))
        grep -o 'boundary="[^"]*boundary-LibPST-[^"]*"' "$eml" | sed 's/^boundary="//; s/"$//' | sort -u >"$work/boundaries"
        while IFS= read -r b; do
            if ! grep -Fqx -- "--$b--" "$eml"; then
                echo "FAIL: $pst: ${eml#$out/}: multipart boundary $b is never closed (message cut short)"
                exit 1
            fi
        done <"$work/boundaries" || exit 1
    done <"$work/emls" || exit 1
    echo "PASS: $pst: $n .eml files${valgrind:+, valgrind clean}"
done || fail=1

exit $fail
