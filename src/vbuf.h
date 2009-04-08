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

vbuf  *pst_vballoc(size_t len);
void   pst_vbgrow(vbuf *vb, size_t len);    // grow buffer by len bytes, data are preserved
void   pst_vbset(vbuf *vb, void *data, size_t len);
void   pst_vbappend(vbuf *vb, void *data, size_t length);
void   pst_unicode_init();
size_t pst_vb_utf16to8(vbuf *dest, const char *inbuf, int iblen);
size_t pst_vb_utf8to8bit(vbuf *dest, const char *inbuf, int iblen, const char* charset);
size_t pst_vb_8bit2utf8(vbuf *dest, const char *inbuf, int iblen, const char* charset);


#endif // VBUF_H
