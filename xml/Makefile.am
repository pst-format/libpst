EXTRA_DIST = Doxyfile $(wildcard *.pdf)

all: libpst
	cat header.xml	libpst >libpst.xml
	cat header.sgml libpst >libpst.sgml
	rm -f ../html/*html
	rm -rf ../html/devel
	xmlto		 -o ../man	man	  libpst.xml
	xmlto		 -o ../html xhtml libpst.xml
	rm -f libpst.xml libpst.sgml
	mkdir tmp
	mkdir tmp/libpst
	cp ../src/common.h ../src/libpst.h ../src/libstrfunc.h ../src/lzfu.h ../src/timeconv.h ../src/vbuf.h tmp/libpst
	doxygen
	rm -rf tmp
