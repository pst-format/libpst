#!/bin/sh
# Regression test: readpst must produce the same output with parallel jobs
# as without them.
#
# readpst forks child processes to extract folders and, with -e/-S/-M, single
# messages. A forked child shares the parent's open file description, and so
# its file offset. When a child's stdio closed or synced its copy of the PST
# stream (glibc freopen() since 2.29, musl fclose()), it moved that shared
# offset between the parent's seek and read, and the parent read the wrong
# bytes: whole messages or folders went missing, or parts of messages (an
# HTML body, an embedded message) were silently dropped, and readpst still
# exited 0. Which item was hit changed from run to run.
#
# For every PST in $LIBPST_TEST_PST (a .pst file, or a directory of them), the
# test runs "readpst -e -8" once with -j 0 as the reference, then
# $LIBPST_TEST_RUNS times (default 20) with the default number of jobs and
# $LIBPST_TEST_RUNS times with -j 0. It fails if any run does not exit 0, or
# if any run's output differs from the reference. Before comparing, the MIME
# boundaries (random in older releases) and the DTSTAMP lines (the current
# time when an item has no creation date) are normalized.
#
# The race is timing dependent, so a single pass proves little about an
# unfixed readpst; raise LIBPST_TEST_RUNS to look harder.
#
# Run directly: READPST=src/readpst LIBPST_TEST_PST=... tests/readpst-parallel-consistency.sh
# Exit status 77 means skipped (no PST given), as automake expects.

READPST=${READPST:-src/readpst}
LIBPST_TEST_RUNS=${LIBPST_TEST_RUNS:-20}

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

work=$(mktemp -d "${TMPDIR:-.}/readpst-test.XXXXXX") || exit 1
trap 'rm -rf "$work"' 0 INT TERM

# extract <dir> [readpst options...]: run readpst into <dir>, normalize the
# output, and return readpst's exit status
extract() {
    dir=$1
    shift
    rm -rf "$dir"
    mkdir "$dir"
    "$READPST" "$@" -e -8 -o "$dir" "$pst" >"$work/stdout" 2>"$work/stderr"
    rc=$?
    find "$dir" -type f | while IFS= read -r f; do
        LC_ALL=C sed -e 's/boundary-LibPST-iamunique-[0-9]*_-_-/boundary-LibPST-iamunique-N_-_-/g' \
                     -e 's/^DTSTAMP:.*/DTSTAMP:N/' "$f" >"$work/normalized" &&
        cat "$work/normalized" >"$f"
    done
    return $rc
}

fail=0
echo "$psts" | while IFS= read -r pst; do
    if ! extract "$work/ref" -j 0; then
        echo "FAIL: $pst: readpst -j 0 exited $rc"
        cat "$work/stderr"
        exit 1
    fi
    nref=$(find "$work/ref" -type f | wc -l)
    bad=0
    for jobs in default 0; do
        i=0
        while [ "$i" -lt "$LIBPST_TEST_RUNS" ]; do
            i=$((i+1))
            if [ "$jobs" = default ]; then
                extract "$work/out"
            else
                extract "$work/out" -j 0
            fi
            n=$(find "$work/out" -type f | wc -l)
            if [ "$rc" -ne 0 ]; then
                echo "FAIL: $pst: -j $jobs run $i: readpst exited $rc ($n of $nref files)"
                sed 's/^/    /' "$work/stderr"
                bad=$((bad+1))
            elif ! diff -rq "$work/ref" "$work/out" >"$work/diff" 2>&1; then
                echo "FAIL: $pst: -j $jobs run $i: output differs from -j 0 ($n of $nref files)"
                sed -e "s|^Only in $work/ref/*\(.*\): |    missing: \1/|" \
                    -e "s|^Only in $work/out/*\(.*\): |    extra: \1/|" \
                    -e "s|^Files $work/ref/\(.*\) and $work/out/.* differ\$|    differs: \1|" \
                    -e "s|^    missing: /|    missing: |; s|^    extra: /|    extra: |" \
                    "$work/diff" | head -n 10
                bad=$((bad+1))
            fi
        done
    done
    if [ "$bad" -ne 0 ]; then
        echo "FAIL: $pst: $bad of $((2*LIBPST_TEST_RUNS)) runs failed"
        exit 1
    fi
    echo "PASS: $pst: $((2*LIBPST_TEST_RUNS)) runs ($LIBPST_TEST_RUNS with default jobs, $LIBPST_TEST_RUNS with -j 0), $nref files, all identical"
done || fail=1

exit $fail
