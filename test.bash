#!/bin/bash

#scp ams:/usr/usr/USR/USR/STAN/NNdbase.txt .
#scp ams:/usr/usr/USR/USR/STAN/outlook.pst .

make
f=/home/ldap/ams.ldif
./pst2ldif -b 'o=ams-cc.com, c=US' -c 'newPerson' outlook.pst >$f
dos2unix <NNdbase.txt | ./nick2ldif -b 'o=ams-cc.com, c=US' -c 'newPerson' >>$f
less $f
(
    cd /home/ldap
    /etc/rc.d/init.d/ldap stop
    d=data/ams-cc.com
    rm -rf $d
    mkdir  $d
    chown ldap:ldap $d
    /etc/rc.d/init.d/ldap start
    sleep 5
    ldapadd -c -x -D 'cn=root, o=ams-cc.com, c=US' -w 'summer-unmade'  -v     <$f >$f.log 2>&1
    ldapsearch -z 0 -x -D 'cn=root, o=ams-cc.com, c=US' -w 'summer-unmade'  -v -b 'o=ams-cc.com, c=US'    'objectclass=*' | grep -v '^#' >$f.all

)
