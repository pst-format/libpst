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

#include "common.h"

#define SZ_MAX     4096
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

#define VBUF_STATIC(x,y) static vbuf *x = NULL; if(!x) x = pst_vballoc(y);
#define VSTR_STATIC(x,y) static vstr *x = NULL; if(!x) x = pst_vsalloc(y);

int pst_skip_nl( char *s );  // returns the width of the newline at s[0]
int pst_find_nl( vstr *vs ); // find newline of type type in b

// vbuf functions
vbuf *pst_vballoc(    size_t len );
void pst_vbfree(      vbuf *vb );
void pst_vbclear(     vbuf *vb ); //ditch the data, keep the buffer
void pst_vbresize(    vbuf *vb, size_t len );
size_t pst_vbavail(   vbuf *vb );
void pst_vbdump(      vbuf *vb );
void pst_vbgrow(      vbuf *vb, size_t len ); // grow buffer by len bytes, data are preserved
void pst_vbset(       vbuf *vb, void *data, size_t len );
void pst_vbskipws(    vbuf *vb );
void pst_vbappend(    vbuf *vb, void *data, size_t length );
void pst_vbskip(      vbuf *vb, size_t skip );
void pst_vboverwrite( vbuf *vbdest, vbuf *vbsrc );

// vstr functions
vstr *pst_vsalloc( size_t len );
char *pst_vsb(      vstr *vs );
size_t pst_vslen(     vstr *vs ); //strlen
void pst_vsfree(      vstr *vs );
void pst_vsset(       vstr *vs, char *s ); // Store string s in vb
void pst_vsnset(      vstr *vs, char *s, size_t n ); // Store string s in vb
void pst_vsgrow(      vstr *vs, size_t len ); // grow buffer by len bytes, data are preserved
size_t pst_vsavail(   vstr *vs );
void pst_vscat(       vstr *vs, char *str );
void pst_vsncat(      vstr *vs, char *str, size_t len );
void pst_vsnprepend(  vstr *vs, char *str, size_t len ) ;
void pst_vsskip(      vstr *vs, size_t len );
int  pst_vscmp(       vstr *vs, char *str );
void pst_vsskipws(    vstr *vs );
void pst_vs_printf(   vstr *vs, char *fmt, ... );
void pst_vs_printfa(  vstr *vs, char *fmt, ... );
void pst_vshexdump(   vstr *vs, const char *b, size_t start, size_t stop, int ascii );
int  pst_vscatprintf( vstr *vs, char *fmt, ... );
void pst_vsvprintf(   vstr *vs, char *fmt, va_list ap );
void pst_vstrunc(     vstr *vs, size_t off ); // Drop chars [off..dlen]
int  pst_vslast(      vstr *vs ); // returns the last character stored in a vstr string
void pst_vscharcat(   vstr *vs, int ch );


void pst_unicode_init();
void pst_unicode_close();
size_t pst_vb_utf16to8(vbuf *dest, const char *inbuf, int iblen);
size_t pst_vb_utf8to8bit(vbuf *dest, const char *inbuf, int iblen, const char* charset);
size_t pst_vb_8bit2utf8(vbuf *dest, const char *inbuf, int iblen, const char* charset);

int pst_vb_skipline( struct varbuf *vb ); // in: vb->b == "stuff\nmore_stuff"; out: vb->b == "more_stuff"


#endif // VBUF_H
