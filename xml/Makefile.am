all: dnsbl
	   cat header.xml  libpst >libpst.xml
	   cat header.sgml libpst >libpst.sgml
	   rm -f ../html/*html
	   rm -f ../html/*pdf
	   xmlto        -o ../man  man   libpst.xml
	   xmlto        -o ../html xhtml libpst.xml
	   xmlto        -o ../html pdf   libpst.xml
	   docbook2texi -o ../info       libpst.sgml
	   rm -f libpst.xml libpst.sgml
