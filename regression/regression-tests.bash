#!/bin/bash


function consistency()
{
    # check source and xml documentation for consistency
    (
        cd ..   # back to top level of project
        f1=/tmp/f1$$
        f2=/tmp/f2$$
        grep 'case 0x' src/libpst.c   | awk '{print $2}' | tr A-Z a-z | sed -e 's/://g'             | sort >$f1
        grep '^0x'     xml/libpst.in  | awk '{print $1}' | (for i in {1..19}; do read a; done; cat) | sort >$f2
        diff $f1 $f2
        less $f1
        #rm -f $f1 $f2
        ls -al $f1 $f2
    )
}

function dodii()
{
    n="$1"
    fn="$2"
    ba=$(basename "$fn" .pst)
    size=$(stat -c %s $fn)
    rm -rf output$n
    if [ -z "$val" ] || [ $size -lt 10000000 ]; then
        echo $fn
        mkdir output$n
        $val ../src/pst2dii -f /usr/share/fonts/liberation/LiberationMono-Regular.ttf -B "bates-" -o output$n -O $ba.mydii -d $fn.log $fn >$fn.dii.err 2>&1
    fi
}


function doldif()
{
    n="$1"
    fn="$2"
    ba=$(basename "$fn" .pst)
    size=$(stat -c %s $fn)
    rm -rf output$n
    if [ -z "$val" ] || [ $size -lt 10000000 ]; then
        echo $fn
        mkdir output$n
        $val ../src/pst2ldif -d $ba.ldif.log -b 'o=ams-cc.com, c=US' -c 'inetOrgPerson' $fn >$ba.ldif.err 2>&1
    fi
}


function dopst()
{
    n="$1"
    fn="$2"
    ba=$(basename "$fn" .pst)
    size=$(stat -c %s $fn)
    jobs=""
    [ -n "$val" ] && jobs="-j 0"
    rm -rf output$n
    if [ -z "$val" ] || [ $size -lt 100000000 ]; then
        echo $fn
        mkdir output$n
        if [ "$regression" == "yes" ]; then
            $val ../src/readpst $jobs -te -r -cv -o output$n $fn >$ba.err 2>&1
        else
            ## only email and include deleted items, have a deleted items folder with multiple item types
            #$val ../src/readpst $jobs -te -r -D -cv -o output$n -d $ba.log $fn >$ba.err 2>&1

            ## normal recursive dump
            char='BIG-5'
            char='us-ascii'
            acc="-a '.xls,.doc'"
            acc=''
            utf='-8'

            ## normal mode
            #echo $val ../src/readpst $utf $acc -C $char -j 0 -cv -o output$n -d $ba.log $fn
            #     $val ../src/readpst $utf $acc -C $char -j 0 -cv -o output$n -d $ba.log $fn >$ba.err 2>&1

            ## kmail mode
            #echo $val ../src/readpst $utf $acc -C $char -j 0 -k -cv -o output$n -d $ba.log $fn
            #     $val ../src/readpst $utf $acc -C $char -j 0 -k -cv -o output$n -d $ba.log $fn >$ba.err 2>&1

            ## recursive mode
            echo $val ../src/readpst $utf $acc -C $char -j 0 -r -cv -o output$n -d $ba.log $fn
                 $val ../src/readpst $utf $acc -C $char -j 0 -r -cv -o output$n -d $ba.log $fn >$ba.err 2>&1

            ## separate mode with filename extensions and .msg files
            #echo $val ../src/readpst $jobs     -r -m -D -cv -o output$n -d $ba.log $fn
            #     $val ../src/readpst $jobs     -r -m -D -cv -o output$n -d $ba.log $fn >$ba.err 2>&1

            ## separate mode where we decode all attachments to binary files
            #echo $val ../src/readpst $jobs      -r -S -D -cv -o output$n -d $ba.log $fn
            #     $val ../src/readpst $jobs      -r -S -D -cv -o output$n -d $ba.log $fn >$ba.err 2>&1

            ## testing idblock
            #../src/getidblock -p $fn 0 >$ba.fulldump
        fi
    fi
}


#consistency
#exit


pushd ..
make || exit
popd

rm -rf output* *.err *.log

v="valgrind --leak-check=full"
val=""

func="dopst"
[ "$1" == "pst"  ] && func="dopst"
[ "$1" == "pstv" ] && func="dopst" && val=$v
[ "$1" == "ldif" ] && func="doldif"
[ "$1" == "dii"  ] && func="dodii"

regression=""
[ "$2" == "reg" ] && regression="yes"
[ "$regression" == "yes" ] && val=""

$func   1 ams.pst
$func   2 sample_64.pst
$func   3 test.pst
$func   4 big_mail.pst
$func   5 mbmg.archive.pst
$func   6 Single2003-read.pst
$func   7 Single2003-unread.pst
$func   8 ol2k3high.pst
$func   9 ol97high.pst
$func  10 returned_message.pst
$func  11 flow.pst
$func  12 test-html.pst
$func  13 test-text.pst
$func  14 joe.romanowski.pst
$func  15 hourig1.pst
$func  16 test-mac.pst
$func  17 backup.pst
$func  18 spam.pst
$func  19 rendgen.pst           # single email appointment
$func  20 rendgen2.pst          # email appointment with no termination date
$func  21 rendgen3.pst          # mime signed email
$func  22 rendgen4.pst          # appointment test cases
$func  23 rendgen5.pst          # appointment test cases
$func  24 paul.sheer.pst        # embedded rfc822 attachment
$func  25 jerry.pst             # non ascii subject lines
$func  26 phill.bertolus.pst    # possible segfault in forked process, cannot reproduce
$func  27 kaiser.pst            # appointments with other character sets
$func  28 pstsample.pst         # character set issue
$func  29 pstsample2.pst        # embedded image in rtf data
$func  30 pstsample3.pst        # exports of rtf and html
$func  31 Journal_Archives_08_29_2010.pst

[ -n "$val" ] && grep 'lost:' *err | grep -v 'lost: 0 '

if [ "$regression" == "yes" ]; then
    (
        (for i in output*; do find $i -type f; done) | while read a; do
            grep -v iamunique "$a"
            rm -f "$a"
        done
    ) >regression.txt
fi
