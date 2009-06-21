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
        rm -f $f1 $f2
    )
}

function dodii()
{
    n="$1"
    fn="$2"
    echo $fn
    rm -rf output$n
    mkdir output$n
    $val ../src/pst2dii -f /usr/share/fonts/bitstream-vera/VeraMono.ttf -B "bates-" -o output$n -O mydii$n -d $fn.log $fn >$fn.dii.err 2>&1
    rm -f dumper
}


function doldif()
{
    n="$1"
    fn="$2"
    echo $fn
    ba=$(basename "$fn" .pst)
    rm -rf output$n
    mkdir output$n
    $val ../src/pst2ldif -d $ba.ldif.log -b 'o=ams-cc.com, c=US' -c 'inetOrgPerson' $fn >$ba.ldif.err 2>&1
    rm -f dumper
}


function dopst()
{
    n="$1"
    fn="$2"
    echo $fn
    ba=$(basename "$fn" .pst)
    jobs=""
    [ -n "$val" ] && jobs="-j 0"
    rm -rf output$n
    mkdir output$n
    #val ../src/readpst $jobs -r -D -cv -o output$n            $fn
    $val ../src/readpst $jobs -r -D -cv -o output$n -d $ba.log $fn >$ba.err 2>&1
    #../src/getidblock -p $fn 0 >$ba.fulldump
}



val="valgrind --leak-check=full"
val=''

pushd ..
make || exit
popd

rm -rf output* *.err *.log

if [ "$1" == "dii" ]; then
    dodii 1 ams.pst
    dodii 2 sample_64.pst
    dodii 3 test.pst
    dodii 4 big_mail.pst
elif [ "$1" == "ldif" ]; then
    doldif   1 ams.pst
    doldif   2 sample_64.pst
    doldif   3 test.pst
    doldif   4 big_mail.pst
    doldif   5 mbmg.archive.pst
    doldif   6 Single2003-read.pst
    doldif   7 Single2003-unread.pst
    doldif   8 ol2k3high.pst
    doldif   9 ol97high.pst
    doldif  10 returned_message.pst
    doldif  11 flow.pst
    doldif  12 test-html.pst
    doldif  13 test-text.pst
    doldif  14 joe.romanowski.pst
    doldif  15 hourig1.pst
    doldif  16 hourig2.pst
    doldif  17 hourig3.pst
    doldif  18 test-mac.pst
    doldif  19 harris.pst
    doldif  20 spam.pst
    doldif  21 rendgen.pst
else
    dopst   1 ams.pst
    dopst   2 sample_64.pst
    dopst   3 test.pst
    dopst   4 big_mail.pst
    dopst   5 mbmg.archive.pst
    dopst   6 Single2003-read.pst
    dopst   7 Single2003-unread.pst
    dopst   8 ol2k3high.pst
    dopst   9 ol97high.pst
    dopst  10 returned_message.pst
    dopst  11 flow.pst
    dopst  12 test-html.pst
    dopst  13 test-text.pst
    dopst  14 joe.romanowski.pst
    dopst  15 hourig1.pst
    #dopst  16 hourig2.pst
    #dopst  17 hourig3.pst
    dopst  18 test-mac.pst
    #dopst  19 harris.pst
    dopst  20 spam.pst
    dopst  21 rendgen.pst       # single email appointment
    dopst  22 rendgen2.pst      # email appointment with no termination date
    dopst  23 rendgen3.pst      # mime signed email
    dopst  24 rendgen4.pst      # appointment test cases
    dopst  25 rendgen5.pst      # appointment test cases
fi

grep 'lost:' *err | grep -v 'lost: 0 '
