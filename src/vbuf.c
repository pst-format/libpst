
#include "common.h"
#include "vbuf.h"

#include "define.h"


#define ASSERT(x,...) { if( !(x) ) DIE(( __VA_ARGS__)); }


int skip_nl(char *s)
{
    if (s[0] == '\n')
        return 1;
    if (s[0] == '\r' && s[1] == '\n')
        return 2;
    if (s[0] == '\0')
        return 0;
    return -1;
}


int find_nl(vstr * vs)
{
    char *nextr, *nextn;

    nextr = memchr(vs->b, '\r', vs->dlen);
    nextn = memchr(vs->b, '\n', vs->dlen);

    //case 1: UNIX, we find \n first
    if (nextn && (!nextr || (nextr > nextn))) {
        return nextn - vs->b;
    }
    //case 2: DOS, we find \r\n
    if (nextr && nextn && (nextn-nextr == 1)) {
        return nextr - vs->b;
    }
    //case 3: we find nothing

    return -1;
}


//  UTF8 <-> UTF16 <-> ISO8859 Character set conversion functions and (ack) their globals

static int unicode_up = 0;
static iconv_t i16to8;
static const char *target_charset = NULL;
static iconv_t    i8totarget;


void unicode_init()
{
    if (unicode_up) unicode_close();
    i16to8 = iconv_open("UTF-8", "UTF-16LE");
    if (i16to8 == (iconv_t)-1) {
        fprintf(stderr, "Couldn't open iconv descriptor for UTF-16LE to UTF-8.\n");
        exit(1);
    }
    unicode_up = 1;
}


void unicode_close()
{
    iconv_close(i16to8);
    if (target_charset) {
        iconv_close(i8totarget);
        free((char *)target_charset);
        target_charset = NULL;
    }
    unicode_up = 0;
}


int utf16_is_terminated(const char *str, int length)
{
    VSTR_STATIC(errbuf, 100);
    int len = -1;
    int i;
    for (i = 0; i < length; i += 2) {
        if (str[i] == 0 && str[i + 1] == 0) {
            len = i;
        }
    }

    if (-1 == len) {
        vshexdump(errbuf, str, 0, length, 1);
        DEBUG_WARN(("String is not zero terminated (probably broken data from registry) %s.\n", errbuf->b));
    }

    return (-1 == len) ? 0 : 1;
}


size_t vb_utf16to8(vbuf *dest, const char *inbuf, int iblen)
{
    size_t inbytesleft  = iblen;
    size_t icresult     = (size_t)-1;
    size_t outbytesleft = 0;
    char *outbuf        = NULL;

    ASSERT(unicode_up, "vb_utf16to8() called before unicode started.");

    if (2 > dest->blen) vbresize(dest, 2);
    dest->dlen = 0;

    //Bad Things can happen if a non-zero-terminated utf16 string comes through here
    if (!utf16_is_terminated(inbuf, iblen))
        return (size_t)-1;

    do {
        outbytesleft = dest->blen - dest->dlen;
        outbuf = dest->b + dest->dlen;
        icresult = iconv(i16to8, (ICONV_CONST char**)&inbuf, &inbytesleft, &outbuf, &outbytesleft);
        dest->dlen = outbuf - dest->b;
        vbgrow(dest, inbytesleft);
    } while ((size_t)-1 == icresult && E2BIG == errno);

    if (icresult == (size_t)-1) {
        DEBUG_WARN(("iconv failure: %s\n", strerror(errno)));
        unicode_init();
        return (size_t)-1;
    }
    return (icresult) ? (size_t)-1 : 0;
}


size_t vb_utf8to8bit(vbuf *dest, const char *inbuf, int iblen, const char* charset)
{
    size_t inbytesleft  = iblen;
    size_t icresult     = (size_t)-1;
    size_t outbytesleft = 0;
    char *outbuf        = NULL;

    if (!target_charset || (target_charset && strcasecmp(target_charset, charset))) {
        if (target_charset) {
            iconv_close(i8totarget);
            free((char *)target_charset);
        }
        target_charset = strdup(charset);
        i8totarget = iconv_open(target_charset, "UTF-8");
        if (i8totarget == (iconv_t)-1) {
            fprintf(stderr, "Couldn't open iconv descriptor for UTF-8 to %s.\n", target_charset);
            return (size_t)-1;
        }
    }

    if (2 > dest->blen) vbresize(dest, 2);
    dest->dlen = 0;

    do {
        outbytesleft = dest->blen - dest->dlen;
        outbuf = dest->b + dest->dlen;
        icresult = iconv(i8totarget, (ICONV_CONST char**)&inbuf, &inbytesleft, &outbuf, &outbytesleft);
        dest->dlen = outbuf - dest->b;
        vbgrow(dest, 20);
    } while ((size_t)-1 == icresult && E2BIG == errno);

    if (icresult == (size_t)-1) {
        WARN(("iconv failure: %s\n", strerror(errno)));
        unicode_init();
        return (size_t)-1;
    }
    return (icresult) ? (size_t)-1 : 0;
}


vbuf *vballoc(size_t len)
{
    struct varbuf *result = malloc(sizeof(struct varbuf));
    if (result) {
        result->dlen = 0;
        result->blen = 0;
        result->buf = NULL;
        vbresize(result, len);
    }
    else DIE(("malloc() failure"));
    return result;
}


void vbcheck(vbuf * vb)
{
    ASSERT(vb->b >= vb->buf, "vbcheck(): data not inside buffer");
    ASSERT((size_t)(vb->b - vb->buf) <= vb->blen, "vbcheck(): vb->b outside of buffer range.");
    ASSERT(vb->dlen <= vb->blen, "vbcheck(): data length > buffer length.");
    ASSERT(vb->blen < 1024 * 1024, "vbcheck(): blen is a bit large...hmmm.");
}


void vbfree(vbuf * vb)
{
    free(vb->buf);
    free(vb);
}


void vbclear(struct varbuf *vb) // ditch the data, keep the buffer
{
    vbresize(vb, 0);
}


void vbresize(struct varbuf *vb, size_t len)    // DESTRUCTIVELY grow or shrink buffer
{
    vb->dlen = 0;

    if (vb->blen >= len) {
        vb->b = vb->buf;
        return;
    }

    vb->buf  = realloc(vb->buf, len);
    vb->b    = vb->buf;
    vb->blen = len;
}


size_t vbavail(vbuf * vb)
{
    return vb->blen  - vb->dlen - (size_t)(vb->b - vb->buf);
}


//void vbdump( vbuf *vb ) // TODO: to stdout?  Yuck
//{
//      printf("vb dump-------------\n");
//        printf("dlen: %d\n", vb->dlen );
//      printf("blen: %d\n", vb->blen );
//      printf("b - buf: %d\n", vb->b - vb->buf );
//      printf("buf:\n");
//      hexdump( vb->buf, 0, vb->blen, 1 );
//      printf("b:\n");
//      hexdump( vb->b, 0, vb->dlen, 1 );
//      printf("^^^^^^^^^^^^^^^^^^^^\n");
//}


void vbgrow(struct varbuf *vb, size_t len)      // out: vbavail(vb) >= len, data are preserved
{
    if (0 == len)
        return;

    if (0 == vb->blen) {
        vbresize(vb, len);
        return;
    }

    if (vb->dlen + len > vb->blen) {
        if (vb->dlen + len < vb->blen * 1.5)
            len = vb->blen * 1.5;
        char *nb = malloc(vb->blen + len);
        if (!nb) DIE(("malloc() failure"));
        vb->blen = vb->blen + len;
        memcpy(nb, vb->b, vb->dlen);

        free(vb->buf);
        vb->buf = nb;
        vb->b = vb->buf;
    } else {
        if (vb->b != vb->buf)
            memcpy(vb->buf, vb->b, vb->dlen);
    }

    vb->b = vb->buf;

    ASSERT(vbavail(vb) >= len, "vbgrow(): I have failed in my mission.");
}


void vbset(vbuf * vb, void *b, size_t len)      // set vbuf b size=len, resize if necessary, relen = how much to over-allocate
{
    vbresize(vb, len);

    memcpy(vb->b, b, len);
    vb->dlen = len;
}


void vsskipws(vstr * vs)
{
    char *p = vs->b;
    while ((size_t)(p - vs->b) < vs->dlen && isspace(p[0]))
        p++;

    vbskip((vbuf *) vs, p - vs->b);
}


// append len bytes of b to vbuf, resize if necessary
void vbappend(struct varbuf *vb, void *b, size_t len)
{
    if (0 == vb->dlen) {
        vbset(vb, b, len);
        return;
    }
    vbgrow(vb, len);
    memcpy(vb->b + vb->dlen, b, len);
    vb->dlen += len;
}


// dumps the first skip bytes from vbuf
void vbskip(struct varbuf *vb, size_t skip)
{
    ASSERT(skip <= vb->dlen, "vbskip(): Attempt to seek past end of buffer.");
    vb->b += skip;
    vb->dlen -= skip;
}


// overwrite vbdest with vbsrc
void vboverwrite(struct varbuf *vbdest, struct varbuf *vbsrc)
{
    vbresize(vbdest, vbsrc->blen);
    memcpy(vbdest->b, vbsrc->b, vbsrc->dlen);
    vbdest->blen = vbsrc->blen;
    vbdest->dlen = vbsrc->dlen;
}


vstr *vsalloc(size_t len)
{
    vstr *result = (vstr *) vballoc(len + 1);
    vsset(result, "");
    return result;
}


char *vsstr(vstr * vs)
{
    return vs->b;
}


size_t vslen(vstr * vs)
{
    return strlen(vsstr(vs));
}


void vsfree(vstr * vs)
{
    vbfree((vbuf *) vs);
}


void vscharcat(vstr * vb, int ch)
{
    vbgrow((vbuf *) vb, 1);
    vb->b[vb->dlen - 1] = ch;
    vb->b[vb->dlen] = '\0';
    vb->dlen++;
}


// prependappend string str to vbuf, vbuf must already contain a valid string
void vsnprepend(vstr * vb, char *str, size_t len)
{
    ASSERT(vb->b[vb->dlen - 1] == '\0', "vsncat(): attempt to append string to non-string.");
    size_t sl = strlen(str);
    size_t n = (sl < len) ? sl : len;
    vbgrow((vbuf *) vb, n + 1);
    memmove(vb->b + n, vb->b, vb->dlen - 1);
    memcpy(vb->b, str, n);
    vb->dlen += n;
    vb->b[vb->dlen - 1] = '\0';
}


// len < dlen-1 -> skip len chars, else DIE
void vsskip(vstr * vs, size_t len)
{
    ASSERT(len < vs->dlen - 1, "Attempt to skip past end of string");
    vbskip((vbuf *) vs, len);
}


// in: vb->b == "stuff\nmore_stuff"; out: vb->b == "more_stuff"
int vsskipline(vstr * vs)
{
    int nloff = find_nl(vs);
    int nll   = skip_nl(vs->b + nloff);

    if (nloff < 0) {
        //TODO: error
        printf("vb_skipline(): there seems to be no newline here.\n");
        return -1;
    }
    if (nll < 0) {
        //TODO: error
        printf("vb_skipline(): there seems to be no newline here...except there should be. :P\n");
        return -1;
    }

    memmove(vs->b, vs->b + nloff + nll, vs->dlen - nloff - nll);

    vs->dlen -= nloff + nll;

    return 0;
}


int vscatprintf(vstr * vs, char *fmt, ...)
{
    int size;
    va_list ap;

    /* Guess we need no more than 100 bytes. */
    //vsresize( vb, 100 );
    if (!vs->b || vs->dlen == 0) {
        vsset(vs, "");
    }

    while (1) {
        /* Try to print in the allocated space. */
        va_start(ap, fmt);
        size = vsnprintf(vs->b + vs->dlen - 1, vs->blen - vs->dlen, fmt, ap);
        va_end(ap);

        /* If that worked, return the string. */
        if ((size > -1) && ((size_t)size < vs->blen - vs->dlen)) {
            vs->dlen += size;
            return size;
        }
        /* Else try again with more space. */
        if (size >= 0)          /* glibc 2.1 */
            vbgrow((vbuf *) vs, size + 1);      /* precisely what is needed */
        else                    /* glibc 2.0 */
            vbgrow((vbuf *) vs, vs->blen);
    }
}


//  returns the last character stored in a vstr
int vslast(vstr * vs)
{
    if (vs->dlen < 1)
        return -1;
    if (vs->b[vs->dlen - 1] != '\0')
        return -1;
    if (vs->dlen == 1)
        return '\0';
    return vs->b[vs->dlen - 2];
}


//  print over vb
void vs_printf(vstr * vs, char *fmt, ...)
{
    int size;
    va_list ap;

    /* Guess we need no more than 100 bytes. */
    vbresize((vbuf *) vs, 100);

    while (1) {
        /* Try to print in the allocated space. */
        va_start(ap, fmt);
        size = vsnprintf(vs->b, vs->blen, fmt, ap);
        va_end(ap);

        /* If that worked, return the string. */
        if ((size > -1) && ((size_t)size < vs->blen)) {
            vs->dlen = size + 1;
            return;
        }
        /* Else try again with more space. */
        if (size >= 0)          /* glibc 2.1 */
            vbresize((vbuf *) vs, size + 1);    /* precisely what is needed */
        else                    /* glibc 2.0 */
            vbresize((vbuf *) vs, vs->blen * 2);
    }
}


// printf append to vs
void vs_printfa(vstr * vs, char *fmt, ...)
{
    int size;
    va_list ap;

    if (vs->blen - vs->dlen < 50)
        vbgrow((vbuf *) vs, 100);

    while (1) {
        /* Try to print in the allocated space. */
        va_start(ap, fmt);
        size = vsnprintf(vs->b + vs->dlen - 1, vs->blen - vs->dlen + 1, fmt, ap);
        va_end(ap);

        /* If that worked, return the string. */
        if ((size > -1) && ((size_t)size < vs->blen)) {
            vs->dlen += size;
            return;
        }
        /* Else try again with more space. */
        if (size >= 0)          /* glibc 2.1 */
            vbgrow((vbuf *) vs, size + 1 - vs->dlen);   /* precisely what is needed */
        else                    /* glibc 2.0 */
            vbgrow((vbuf *) vs, size);
    }
}


void vshexdump(vstr * vs, const char *b, size_t start, size_t stop, int ascii)
{
    char c;
    int diff, i;

    while (start < stop) {
        diff = stop - start;
        if (diff > 16)
            diff = 16;

        vs_printfa(vs, ":%08X  ", start);

        for (i = 0; i < diff; i++) {
            if (8 == i)
                vs_printfa(vs, " ");
            vs_printfa(vs, "%02X ", (unsigned char) *(b + start + i));
        }
        if (ascii) {
            for (i = diff; i < 16; i++)
                vs_printfa(vs, "   ");
            for (i = 0; i < diff; i++) {
                c = *(b + start + i);
                vs_printfa(vs, "%c", isprint(c) ? c : '.');
            }
        }
        vs_printfa(vs, "\n");
        start += 16;
    }
}


void vsset(vstr * vs, char *s)  // Store string s in vs
{
    vsnset(vs, s, strlen(s));
}


void vsnset(vstr * vs, char *s, size_t n)       // Store string s in vs
{
    vbresize((vbuf *) vs, n + 1);
    memcpy(vs->b, s, n);
    vs->b[n] = '\0';
    vs->dlen = n + 1;
}


void vsgrow(vstr * vs, size_t len)      // grow buffer by len bytes, data are preserved
{
    vbgrow((vbuf *) vs, len);
}


size_t vsavail(vstr * vs)
{
    return vbavail((vbuf *) vs);
}


void vsnset16(vstr * vs, char *s, size_t len)   // Like vbstrnset, but for UTF16
{
    vbresize((vbuf *) vs, len + 1);
    memcpy(vs->b, s, len);

    vs->b[len] = '\0';
    vs->dlen = len + 1;
    vs->b[len] = '\0';
}


void vscat(vstr * vs, char *str)
{
    vsncat(vs, str, strlen(str));
}


int vscmp(vstr * vs, char *str)
{
    return strcmp(vs->b, str);
}


void vsncat(vstr * vs, char *str, size_t len)   // append string str to vstr, vstr must already contain a valid string
{
    ASSERT(vs->b[vs->dlen - 1] == '\0', "vsncat(): attempt to append string to non-string.");
    size_t sl = strlen(str);
    size_t n = (sl < len) ? sl : len;
    //string append
    vbgrow((vbuf *) vs, n + 1);
    memcpy(vs->b + vs->dlen - 1, str, n);
    vs->dlen += n;
    vs->b[vs->dlen - 1] = '\0';
}


void vstrunc(vstr * v, size_t off) // Drop chars [off..dlen]
{
    if (off >= v->dlen - 1)
        return;                 //nothing to do
    v->b[off] = '\0';
    v->dlen = off + 1;
}


