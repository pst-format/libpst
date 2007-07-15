#!/bin/bash

for i in {1..6}; do
    rm -rf output$i
    mkdir output$i
done

#   ../src/pst2ldif -b 'o=ams-cc.com, c=US' -c 'newPerson' ams.pst >ams.err
#   ../src/readpst -cv    -o output1 ams.pst
#   ../src/readpst -cl -r -o output2 ams.pst
#   ../src/readpst -S     -o output3 ams.pst
#   ../src/readpst -M     -o output4 ams.pst
#   ../src/readpst        -o output5 mbmg.archive.pst

    ../src/readpst        -o output1 -d dumper ams.pst
    ../src/readpstlog dumper >dumperams.log

    ../src/readpst        -o output6 -d dumper /tmp/pam.pst
    ../src/readpstlog dumper >dumperpam.log

    rm -f dumper
