#!/bin/sh
# Regression test: when readpst cannot read an item, it must say so on stderr
# and exit with status 3, also when the item was read by a child process.
#
# readpst used to count an item that pst_parse_item() could not read only in
# the folder's "items skipped" total, which also counts items it skips on
# purpose, and it exited 0. And when a child process failed, the parent passed
# the raw wait status to exit(), which keeps only its low 8 bits, so a child's
# non-zero exit code was turned into 0.
#
# For every PST in $LIBPST_TEST_PST (a .pst file, or a directory of them), the
# test makes the PST unreadable in one known place:
#   1. It runs "readpst -j 0 -e -8 -d LOG" on the PST, which must exit 0.
#   2. From the debug log it picks the first email that is not directly in
#      the top folder (so that with parallel jobs a child process reads it),
#      and looks up the file offset and size of that email's descriptor block
#      in the index.
#   3. It copies the PST and overwrites that block with zeros.
# Then it runs readpst on the copy with -j 0 and with the default number of
# jobs, and fails unless both exit 3 and name that item on stderr.
#
# The debug log holds a hex dump of every block readpst reads, so it is many
# times the size of the PST; use a small PST.
#
# Run directly: READPST=src/readpst LIBPST_TEST_PST=... tests/readpst-unreadable-item.sh
# Exit status 77 means skipped (no usable PST given), as automake expects.

READPST=${READPST:-src/readpst}

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

# run <readpst options...>: extract $work/bad.pst into $work/out
run() {
    rm -rf "$work/out"
    mkdir "$work/out"
    "$READPST" "$@" -e -8 -o "$work/out" "$work/bad.pst" >"$work/stdout" 2>"$work/stderr"
}

echo "$psts" | while IFS= read -r pst; do
    rm -rf "$work/out"
    mkdir "$work/out"
    "$READPST" -j 0 -e -8 -d "$work/debug" -o "$work/out" "$pst" >"$work/stdout" 2>"$work/stderr"
    rc=$?
    if [ "$rc" -ne 0 ]; then
        echo "SKIP: $pst: readpst exits $rc on the unmodified file"
        continue
    fi

    # debug lines are "<pid> <indent><function> <file>(<line>) <message>";
    # nested calls of process() are indented further
    item=$(awk '
        / process readpst\.c\([0-9]+\) / {
            match($0, /^[0-9]+ +/)
            indent = RLENGTH
            if (top == "" || indent < top) top = indent
        }
        / process readpst\.c\([0-9]+\) Desc Email ID / {
            s = $0
            sub(/.* Desc Email ID /, "", s)
            sub(/ \[d_ptr->d_id = /, " ", s)
            sub(/\].*/, "", s)
            desc = s
            desc_indent = indent
        }
        / process readpst\.c\([0-9]+\) Processing Email/ {
            if (desc != "" && desc_indent > top) { print desc; exit }
        }' "$work/debug")
    desc_id=${item% *}
    d_id=${item#* }
    block=$(sed -n "s/.* Item \[id = $desc_id, offset = \(0x[0-9a-f]*\), u1 = [-0-9]*, size = \([0-9]*\)(.*/\1 \2/p" "$work/debug" | head -n 1)
    rm -f "$work/debug"
    if [ -z "$item" ] || [ -z "$block" ]; then
        echo "SKIP: $pst: found no email below a subfolder to make unreadable"
        continue
    fi
    offset=$((${block% *}))
    size=${block#* }

    cp "$pst" "$work/bad.pst" || exit 1
    chmod u+w "$work/bad.pst"
    dd if=/dev/zero of="$work/bad.pst" bs=1 seek="$offset" count="$size" conv=notrunc 2>/dev/null || exit 1
    echo "$pst: zeroed $size bytes at offset $offset, the descriptor block of item $d_id"

    for jobs in 0 default; do
        if [ "$jobs" = default ]; then
            run
        else
            run -j "$jobs"
        fi
        rc=$?
        if [ "$rc" -ne 3 ]; then
            echo "FAIL: $pst: -j $jobs: readpst exited $rc, expected 3"
            sed 's/^/    /' "$work/stderr"
            exit 1
        fi
        if ! grep -q "could not read item $d_id " "$work/stderr"; then
            echo "FAIL: $pst: -j $jobs: stderr does not name item $d_id"
            sed 's/^/    /' "$work/stderr"
            exit 1
        fi
    done
    echo "PASS: $pst: readpst exits 3 and names item $d_id, with -j 0 and with default jobs"
    touch "$work/passed"
done || exit 1

if [ ! -f "$work/passed" ]; then
    exit 77
fi
exit 0
