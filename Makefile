GCC_FLAGS=-g ${CFLAGS}

all: readpst pst2ldif nick2ldif getidblock readpstlog dumpblocks

libpst.o: libpst.c libpst.h define.h
	gcc ${GCC_FLAGS} -c libpst.c -o libpst.o

libstrfunc.o: libstrfunc.c libstrfunc.h
	gcc ${GCC_FLAGS} -c libstrfunc.c -o libstrfunc.o

debug.o: debug.c define.h
	gcc ${GCC_FLAGS} -c debug.c -o debug.o

lzfu.o: lzfu.c define.h
	gcc ${GCC_FLAGS} -c lzfu.c -o lzfu.o

readpst: readpst.c define.h libpst.o timeconv.o libstrfunc.o common.h debug.o lzfu.o
#	ccmalloc gcc -Wall -Werror readpst.c -g -o readpst libpst.o timeconv.o libstrfunc.o debug.o
#	gcc -Wall -Werror readpst.c -g -o readpst libpst.o timeconv.o libstrfunc.o debug.o lzfu.o -lefence
	gcc ${GCC_FLAGS} readpst.c -o readpst libpst.o timeconv.o libstrfunc.o debug.o lzfu.o

pst2ldif: pst2ldif.c define.h libpst.o timeconv.o libstrfunc.o common.h debug.o lzfu.o
	gcc ${GCC_FLAGS} pst2ldif.c -o pst2ldif libpst.o timeconv.o libstrfunc.o debug.o lzfu.o

nick2ldif: nick2ldif.cpp define.h libpst.o timeconv.o libstrfunc.o common.h debug.o lzfu.o
	g++ ${GCC_FLAGS} nick2ldif.cpp -o nick2ldif libpst.o timeconv.o libstrfunc.o debug.o lzfu.o

timeconv.o: timeconv.c timeconv.h common.h
	gcc ${GCC_FLAGS} -c timeconv.c -o timeconv.o

getidblock: getidblock.c define.h libpst.o common.h debug.o libstrfunc.o
	gcc ${GCC_FLAGS} getidblock.c -o getidblock libpst.o debug.o timeconv.o libstrfunc.o

testdebug: testdebug.c define.h debug.o
	gcc ${GCC_FLAGS} testdebug.c -o testdebug debug.o libstrfunc.o

readpstlog: readpstlog.c define.h debug.o
	gcc ${GCC_FLAGS} readpstlog.c -g -o readpstlog debug.o libstrfunc.o

dumpblocks: dumpblocks.c define.h libpst.o debug.o
	gcc ${GCC_FLAGS} dumpblocks.c -o dumpblocks libpst.o debug.o libstrfunc.o timeconv.o

clean:
	rm -f core readpst pst2ldif libpst.o timeconv.o libstrfunc.o debug.o getidblock readpstlog testdebug dumpblocks lzfu.o *~

rebuild: clean all

install: all
	cp readpst    /usr/local/bin
	cp pst2ldif   /usr/local/bin
	cp readpstlog /usr/local/bin
uninstall:
	rm -f /usr/local/bin/readpst
	rm -f /usr/local/bin/pst2ldif
	rm -f /usr/local/bin/readpstlog
