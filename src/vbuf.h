/* {{{ vbuf.h - variable length buffer functions
 *
 * Functions that try to make dealing with buffers easier.
 *
 * vbuf
 *
 * vstr
 * - should always contain a valid string
 *
 * }}} */

#ifndef VBUF_H
#define VBUF_H
#define SZ_MAX     4096
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
/***************************************************/

// {{{ Tokenizer const TOK_EMPTY, TOK_ELEMENT, DELIM
#define DELIM '\\'

#define TOK_EMPTY	0
#define TOK_DELIM	1
#define TOK_PARENT	2
#define TOK_CURRENT	3
#define TOK_ELEMENT	4

#define TOK_ERROR	10
#define TOK_BUF_SMALL	11
// }}}


// Variable-length buffers 
struct varbuf { // {{{
	size_t dlen; 	//length of data stored in buffer
	size_t blen; 	//length of buffer
	char *buf; 	//buffer
	char *b;	//start of stored data
}; // }}}


// The exact same thing as a varbuf but should always contain at least '\0' 
struct varstr { // {{{
	size_t dlen; 	//length of data stored in buffer
	size_t blen; 	//length of buffer
	char *buf; 	//buffer
	char *b;	//start of stored data
}; // }}}


typedef struct varbuf vbuf;
typedef struct varstr vstr;

#define VBUF_STATIC(x,y) static vbuf *x = NULL; if(!x) x = vballoc(y);
#define VSTR_STATIC(x,y) static vstr *x = NULL; if(!x) x = vsalloc(y);

// vbuf functions
struct varbuf *vballoc( size_t len );
void vbfree(      vbuf *vb );
void vbclear(     vbuf *vb ); //ditch the data, keep the buffer
void vbresize(    vbuf *vb, size_t len );
int  vbavail(     vbuf *vb );
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
void vshexdump(   vstr *vs, char *b, size_t start, size_t stop, int ascii );
int  vscatprintf( vstr *vs, char *fmt, ... );
void vsvprintf(   vstr *vs, char *fmt, va_list ap );
void vstrunc(     vstr *vs, int off ); // Drop chars [off..dlen]
int  vslast(      vstr *vs ); // returns the last character stored in a vstr string
void vscharcat(   vstr *vs, int ch );
int  vsutf16(     vstr *vs, vbuf *in ); //in: in=zero-terminated utf16; out: vs=utf8; returns: 0 on success, else on fail

int vs_parse_escaped_string( vstr *vs, char *str, size_t len );


/*
 * Windows unicode output trash - this stuff sucks
 * TODO: most of this should not be here
 */

void unicode_init();
void unicode_close();
int utf16_write( FILE* stream, const void *buf, size_t count );
int utf16_fprintf( FILE* stream, const char *fmt, ... );
int utf16to8( char *inbuf_o, char *outbuf_o, int length );
int utf8to16( char *inbuf_o, int iblen, char *outbuf_o, int oblen);
int vb_utf8to16T( vbuf *bout, char *cin, int inlen );
int vb_utf16to8( vbuf *dest, char *buf, int len );
int iso8859_1to8( char *inbuf_o, char *outbuf_o, int length );
int utf8toascii( const char *inbuf_o, char *outbuf_o, int length );

/* dump ascii hex in windoze format */
void winhex(FILE* stream, unsigned char *hbuf, int start, int stop, int loff);
void winhex8(FILE *stream, unsigned char *hbuf, int start, int stop, int loff );

void vbwinhex8(vbuf *vb, unsigned char *hbuf, int start, int stop, int loff );

/* general search routine, find something in something else */
int find_in_buf(char *buf, char *what, int sz, int len, int start);

/* Get INTEGER from memory. This is probably low-endian specific? */
int get_int( char *array );

int find_nl( vstr *vs ); // find newline of type type in b
int skip_nl( char *s ); // returns the width of the newline at s[0]
//int vb_readline( struct varbuf *vb, int *ctype, FILE *in ); // read *AT LEAST* one full line of data from in
int vb_skipline( struct varbuf *vb ); // in: vb->b == "stuff\nmore_stuff"; out: vb->b == "more_stuff"
/* Get a string of HEX bytes (space separated),
 * or if first char is ' get an ASCII string instead.  */
int gethexorstr(char **c, char *wb);
char *esc_index( char *s, int c ); // just like index(3), but works on strings with escape sequences
char *esc_rindex( char *s, int c ); // just like rindex(3), but works on strings with escape sequences

char *tok_esc_char( char *s, int *is_esc, int *c );
int vb_path_token( vbuf *tok, char **path ); // returns things like TOK_EMPTY, TOK_ERROR, complete list at top

int gettoken( char *tok, int len, char **path, char delim ); // Path tokenizer: increments path, dumps token in tok
#endif
