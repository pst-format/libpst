
#ifndef __PST_VBUF_H
#define __PST_VBUF_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Variable-length buffers
struct pst_varbuf {
	size_t dlen; 	//length of data stored in buffer
	size_t blen; 	//length of buffer
	char *buf; 	    //buffer
	char *b;	    //start of stored data
};


typedef struct pst_varbuf pst_vbuf;

pst_vbuf  *pst_vballoc(size_t len);
void       pst_vbgrow(pst_vbuf *vb, size_t len);    // grow buffer by len bytes, data are preserved
void       pst_vbset(pst_vbuf *vb, void *data, size_t len);
void       pst_vbappend(pst_vbuf *vb, void *data, size_t length);
void       pst_unicode_init();
void       pst_unicode_close();
size_t     pst_vb_utf16to8(pst_vbuf *dest, const char *inbuf, int iblen);
size_t     pst_vb_utf8to8bit(pst_vbuf *dest, const char *inbuf, int iblen, const char* charset);
size_t     pst_vb_8bit2utf8(pst_vbuf *dest, const char *inbuf, int iblen, const char* charset);

#ifdef __cplusplus
}
#endif

#endif
