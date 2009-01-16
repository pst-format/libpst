/* vbuf.h - variable length buffer functions
 *
 * Functions that try to make dealing with buffers easier.
 *
 * vbuf
 *
 * vstr
 * - should always contain a valid string
 *
 */

#ifndef VBUF_H
#define VBUF_H
#define SZ_MAX     4096
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
/***************************************************/


// Variable-length buffers
struct varbuf {
	size_t dlen; 	//length of data stored in buffer
	size_t blen; 	//length of buffer
	char *buf; 	    //buffer
	char *b;	    //start of stored data
};


// The exact same thing as a varbuf but should always contain at least '\0'
struct varstr {
	size_t dlen; 	//length of data stored in buffer
	size_t blen; 	//length of buffer
	char *buf; 	    //buffer
	char *b;	    //start of stored data
};


typedef struct varbuf vbuf;
typedef struct varstr vstr;

#define VBUF_STATIC(x,y) static vbuf *x = NULL; if(!x) x = vballoc(y);
#define VSTR_STATIC(x,y) static vstr *x = NULL; if(!x) x = vsalloc(y);

int skip_nl( char *s );  // returns the width of the newline at s[0]
int find_nl( vstr *vs ); // find newline of type type in b

// vbuf functions
struct varbuf *vballoc( size_t len );
void vbfree(      vbuf *vb );
void vbclear(     vbuf *vb ); //ditch the data, keep the buffer
void vbresize(    vbuf *vb, size_t len );
size_t vbavail(   vbuf *vb );
void vbdump(      vbuf *vb );
void vbgrow(      vbuf *vb, size_t len ); // grow buffer by len bytes, data are preserved
void vbset(       vbuf *vb, void *data, size_t len );
void vbskipws(    vbuf *vb );
void vbappend(    vbuf *vb, void *data, size_t length );
void vbskip(      vbuf *vb, size_t skip );
void vboverwrite( vbuf *vbdest, vbuf *vbsrc );

// vstr functions
vstr *vsalloc( size_t len );
char *vsb(      vstr *vs );
size_t vslen(     vstr *vs ); //strlen
void vsfree(      vstr *vs );
void vsset(       vstr *vs, char *s ); // Store string s in vb
void vsnset(      vstr *vs, char *s, size_t n ); // Store string s in vb
void vsgrow(      vstr *vs, size_t len ); // grow buffer by len bytes, data are preserved
size_t vsavail(   vstr *vs );
void vscat(       vstr *vs, char *str );
void vsncat(      vstr *vs, char *str, size_t len );
void vsnprepend(  vstr *vs, char *str, size_t len ) ;
void vsskip(      vstr *vs, size_t len );
int  vscmp(       vstr *vs, char *str );
void vsskipws(    vstr *vs );
void vs_printf(   vstr *vs, char *fmt, ... );
void vs_printfa(  vstr *vs, char *fmt, ... );
void vshexdump(   vstr *vs, const char *b, size_t start, size_t stop, int ascii );
int  vscatprintf( vstr *vs, char *fmt, ... );
void vsvprintf(   vstr *vs, char *fmt, va_list ap );
void vstrunc(     vstr *vs, size_t off ); // Drop chars [off..dlen]
int  vslast(      vstr *vs ); // returns the last character stored in a vstr string
void vscharcat(   vstr *vs, int ch );


void unicode_init();
void unicode_close();
size_t vb_utf16to8(vbuf *dest, const char *inbuf, int iblen);
size_t vb_utf8to8bit(vbuf *dest, const char *inbuf, int iblen, const char* charset);

int vb_skipline( struct varbuf *vb ); // in: vb->b == "stuff\nmore_stuff"; out: vb->b == "more_stuff"
#endif
