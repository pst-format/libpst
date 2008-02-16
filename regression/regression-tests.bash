#!/bin/bash

val="valgrind --leak-check=full"

for i in {1..10}; do
    rm -rf output$i
    mkdir output$i
done


$val  ../src/pst2ldif -b 'o=ams-cc.com, c=US' -c 'newPerson' ams.pst >ams.err  2>&1
$val  ../src/readpst -cv    -o output1 ams.pst                       >out1.err 2>&1
$val  ../src/readpst -cl -r -o output2 ams.pst                       >out2.err 2>&1
$val  ../src/readpst -S     -o output3 ams.pst                       >out3.err 2>&1
$val  ../src/readpst -M     -o output4 ams.pst                       >out4.err 2>&1

$val  ../src/readpst        -o output5 -d dumper mbmg.archive.pst    >out5.err 2>&1
      ../src/readpstlog -f I dumper >mbmg.archive.log

$val  ../src/readpst        -o output6 -d dumper test.pst            >out6.err 2>&1
      ../src/readpstlog -f I dumper >dumpertest.log

$val  ../src/readpst -cv    -o output7 -d dumper sample_64.pst       >out7.err 2>&1
      ../src/readpstlog -f I dumper >sample_64.log

$val  ../src/readpst -cv    -o output8 -d dumper big_mail.pst        >out8.err 2>&1
      ../src/readpstlog -f I dumper >big_mail.log

$val  ../src/readpst -cv    -o output9 -d dumper Single2003-read.pst >out9.err 2>&1
      ../src/readpstlog -f I dumper >Single2003-read.log

$val  ../src/readpst -cv    -o output10 -d dumper Single2003-unread.pst >out10.err 2>&1
      ../src/readpstlog -f I dumper >Single2003-unread.log

$val  ../src/lspst -d dumper ams.pst                                 >out11.err 2>&1
      ../src/readpstlog -f I dumper >ams.log

rm -f dumper

