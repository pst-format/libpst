/***
 * define.h
 * Part of the LibPST project
 * Written by David Smith
 *            dave.s@earthcorp.com
 */

#ifndef DEFINEH_H
#define DEFINEH_H

#include "libpst.h"
#include "timeconv.h"
#include "libstrfunc.h"
#include "vbuf.h"


#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif


#define DEBUG_MODE_GEN
#define DEBUGPRINT
#define DEBUG_MODE_WARN
#define DEBUG_MODE_READ
#define DEBUG_MODE_EMAIL
#define DEBUG_MODE_MAIN
#define DEBUG_MODE_INDEX
#define DEBUG_MODE_CODE
#define DEBUG_MODE_INFO
#define DEBUG_MODE_HEXDUMP
#define DEBUG_MODE_FUNC

//number of items to save in memory between writes
#define DEBUG_MAX_ITEMS 0

#define DEBUG_FILE_NO     1
#define DEBUG_INDEX_NO    2
#define DEBUG_EMAIL_NO    3
#define DEBUG_WARN_NO     4
#define DEBUG_READ_NO     5
#define DEBUG_INFO_NO     6
#define DEBUG_MAIN_NO     7
#define DEBUG_DECRYPT_NO  8
#define DEBUG_FUNCENT_NO  9
#define DEBUG_FUNCRET_NO 10
#define DEBUG_HEXDUMP_NO 11

#ifdef HAVE_TIME_H
    #include <time.h>
#endif

#ifdef HAVE_STRING_H
    #include <string.h>
#endif

#ifdef HAVE_CTYPE_H
    #include <ctype.h>
#endif

#ifdef HAVE_LIMITS_H
    #include <limits.h>
#endif

#ifdef HAVE_WCHAR_H
    #include <wchar.h>
#endif

#ifdef HAVE_SIGNAL_H
    #include <signal.h>
#endif

#ifdef HAVE_ERRNO_H
    #include <errno.h>
#endif

#ifdef HAVE_ICONV
    #include <iconv.h>
#endif

#ifdef HAVE_REGEX_H
    #include <regex.h>
#endif

#ifdef HAVE_GD_H
    #include <gd.h>
#endif


#define PERM_DIRS 0777

#ifdef _WIN32
    #include <direct.h>

    #define D_MKDIR(x) mkdir(x)
    #define chdir      _chdir
    #define strcasecmp _stricmp
    #define vsnprintf  _vsnprintf
    #define snprintf   _snprintf
    #ifdef _MSC_VER
        #define ftello     _ftelli64
        #define fseeko     _fseeki64
    #elif defined (__MINGW32__)
        #define ftello     ftello64
        #define fseeko     fseeko64
    #else
        #error Only MSC and mingw supported for Windows
    #endif
    #ifndef __MINGW32__
        #define size_t     __int64
    #endif
    #ifndef UINT64_MAX
        #define UINT64_MAX ((uint64_t)0xffffffffffffffff)
    #endif
    #define PRIx64 "I64x"
    int __cdecl _fseeki64(FILE *, __int64, int);
    __int64 __cdecl _ftelli64(FILE *);

    #ifdef __MINGW32__
        #include <getopt.h>
    #else
        #include "XGetopt.h"
    #endif
#else
    #ifdef HAVE_UNISTD_H
        #include <unistd.h>
    #else
        #include "XGetopt.h"
    #endif
    #define D_MKDIR(x) mkdir(x, PERM_DIRS)
#endif

#ifdef HAVE_SYS_STAT_H
    #include <sys/stat.h>
#endif

#ifdef HAVE_SYS_TYPES_H
    #include <sys/types.h>
#endif

#ifdef HAVE_DIRENT_H
    #include <dirent.h>
#endif


void pst_debug(const char *fmt, ...);
void pst_debug_hexdumper(FILE* out, char* buf, size_t size, int col, int delta);
void pst_debug_hexprint(char *data, int size);

void pst_debug_init(const char *fname);
void pst_debug_msg_info (int line, const char *file, int type);
void pst_debug_msg_text(const char* fmt, ...);
void pst_debug_hexdump(char *x, size_t y, int cols, int delta);
void pst_debug_func(const char *function);
void pst_debug_func_ret();
void pst_debug_close(void);
void pst_debug_write();
size_t pst_debug_fwrite(const void *ptr, size_t size, size_t nitems, FILE *stream);

void * xmalloc(size_t size);

#define MESSAGEPRINT(x,y) {pst_debug_msg_info(__LINE__,__FILE__,y);\
                           pst_debug_msg_text x;}

#define LOGSTOP() {MESSAGESTOP();DEBUGSTOP();}

#define DIE(x) {\
 MESSAGEPRINT(x, 0);\
 printf x;\
 exit(EXIT_FAILURE);\
}
#define WARN(x) {\
 MESSAGEPRINT(x, 0);\
 printf x;\
}

#ifdef DEBUGPRINT
#define DEBUG_PRINT(x) pst_debug x;
#else
#define DEBUG_PRINT(x) {}
#endif

#ifdef DEBUG_MODE_GEN
#define DEBUG(x) {DEBUG_PRINT(x);}
#else
#define DEBUG(x) {}
#endif

#ifdef DEBUG_MODE_INDEX
#define DEBUG_INDEX(x) MESSAGEPRINT(x, DEBUG_INDEX_NO);
#else
#define DEBUG_INDEX(x) {}
#endif

#ifdef DEBUG_MODE_EMAIL
#define DEBUG_EMAIL(x) MESSAGEPRINT(x, DEBUG_EMAIL_NO);
#define DEBUG_EMAIL_HEXPRINT(x,y) {pst_debug_msg_info(__LINE__, __FILE__, 11);\
                                   pst_debug_hexdump((char*)x, y, 0x10, 0);}
#else
#define DEBUG_EMAIL(x) {}
#define DEBUG_EMAIL_HEXPRINT(x,y) {}
#endif

#ifdef DEBUG_MODE_WARN
#define DEBUG_WARN(x) MESSAGEPRINT(x, DEBUG_WARN_NO);
#else
#define DEBUG_WARN(x) {}
#endif

#ifdef DEBUG_MODE_READ
#define DEBUG_READ(x) MESSAGEPRINT(x, DEBUG_READ_NO);
#else
#define DEBUG_READ(x) {}
#endif

#ifdef DEBUG_MODE_INFO
#define DEBUG_INFO(x) MESSAGEPRINT(x, DEBUG_INFO_NO);
#else
#define DEBUG_INFO(x) {}
#endif

#ifdef DEBUG_MODE_MAIN
#define DEBUG_MAIN(x) MESSAGEPRINT(x, DEBUG_MAIN_NO);
#else
#define DEBUG_MAIN(x) {}
#endif

#ifdef DEBUG_MODE_CODE
#define DEBUG_CODE(x) {x}
#else
#define DEBUG_CODE(x) {}
#endif

#ifdef DEBUG_MODE_DECRYPT
#define DEBUG_DECRYPT(x) MESSAGEPRINT(x, DEBUG_DECRYPT_NO);
#else
#define DEBUG_DECRYPT(x) {}
#endif

#ifdef DEBUG_MODE_HEXDUMP
#define DEBUG_HEXDUMP(x, s)\
  {pst_debug_msg_info(__LINE__, __FILE__, DEBUG_HEXDUMP_NO);\
   pst_debug_hexdump((char*)x, s, 0x10, 0);}
#define DEBUG_HEXDUMPC(x, s, c)\
  {pst_debug_msg_info(__LINE__, __FILE__, DEBUG_HEXDUMP_NO);\
   pst_debug_hexdump((char*)x, s, c, 0);}
#else
#define DEBUG_HEXDUMP(x, s) {}
#define DEBUG_HEXDUMPC(x, s, c) {}
#endif

#define DEBUG_FILE(x) {pst_debug_msg_info(__LINE__, __FILE__, DEBUG_FILE_NO);\
                       pst_debug_msg_text x;}

#ifdef DEBUG_MODE_FUNC
# define DEBUG_ENT(x)                                           \
    {                                                           \
        pst_debug_func(x);                                      \
        MESSAGEPRINT(("Entering function %s\n",x),DEBUG_FUNCENT_NO); \
    }
# define DEBUG_RET()                                            \
    {                                                           \
        MESSAGEPRINT(("Leaving function\n"),DEBUG_FUNCRET_NO);  \
        pst_debug_func_ret();                                   \
    }
#else
# define DEBUG_ENT(x) {}
# define DEBUG_RET() {}
#endif

#define DEBUG_INIT(fname) {pst_debug_init(fname);}
#define DEBUG_CLOSE() {pst_debug_close();}
#define DEBUG_REGISTER_CLOSE() {if(atexit(pst_debug_close)!=0) fprintf(stderr, "Error registering atexit function\n");}

#define RET_DERROR(res, ret_val, x)\
    if (res) { DIE(x);}

#define RET_ERROR(res, ret_val)\
    if (res) {return ret_val;}

#define DEBUG_VERSION 1
struct pst_debug_file_rec_m {
    unsigned short int funcname;
    unsigned short int filename;
    unsigned short int text;
    unsigned short int end;
    unsigned int line;
    unsigned int type;
};

struct pst_debug_file_rec_l {
    unsigned int funcname;
    unsigned int filename;
    unsigned int text;
    unsigned int end;
    unsigned int line;
    unsigned int type;
};

#if BYTE_ORDER == BIG_ENDIAN
#  define LE64_CPU(x) \
  x = ((((x) & UINT64_C(0xff00000000000000)) >> 56) | \
       (((x) & UINT64_C(0x00ff000000000000)) >> 40) | \
       (((x) & UINT64_C(0x0000ff0000000000)) >> 24) | \
       (((x) & UINT64_C(0x000000ff00000000)) >> 8 ) | \
       (((x) & UINT64_C(0x00000000ff000000)) << 8 ) | \
       (((x) & UINT64_C(0x0000000000ff0000)) << 24) | \
       (((x) & UINT64_C(0x000000000000ff00)) << 40) | \
       (((x) & UINT64_C(0x00000000000000ff)) << 56));
#  define LE32_CPU(x) \
  x = ((((x) & 0xff000000) >> 24) | \
       (((x) & 0x00ff0000) >> 8 ) | \
       (((x) & 0x0000ff00) << 8 ) | \
       (((x) & 0x000000ff) << 24));
#  define LE16_CPU(x) \
  x = ((((x) & 0xff00) >> 8) | \
       (((x) & 0x00ff) << 8));
#elif BYTE_ORDER == LITTLE_ENDIAN
#  define LE64_CPU(x) {}
#  define LE32_CPU(x) {}
#  define LE16_CPU(x) {}
#else
#  error "Byte order not supported by this library"
#endif // BYTE_ORDER


#define PST_LE_GET_UINT64(p) \
        (uint64_t)((((uint8_t const *)(p))[0] << 0)  |    \
                   (((uint8_t const *)(p))[1] << 8)  |    \
                   (((uint8_t const *)(p))[2] << 16) |    \
                   (((uint8_t const *)(p))[3] << 24) |    \
                   (((uint8_t const *)(p))[4] << 32) |    \
                   (((uint8_t const *)(p))[5] << 40) |    \
                   (((uint8_t const *)(p))[6] << 48) |    \
                   (((uint8_t const *)(p))[7] << 56))

#define PST_LE_GET_INT64(p) \
        (int64_t)((((uint8_t const *)(p))[0] << 0)  |    \
                  (((uint8_t const *)(p))[1] << 8)  |    \
                  (((uint8_t const *)(p))[2] << 16) |    \
                  (((uint8_t const *)(p))[3] << 24) |    \
                  (((uint8_t const *)(p))[4] << 32) |    \
                  (((uint8_t const *)(p))[5] << 40) |    \
                  (((uint8_t const *)(p))[6] << 48) |    \
                  (((uint8_t const *)(p))[7] << 56))

#define PST_LE_GET_UINT32(p) \
        (uint32_t)((((uint8_t const *)(p))[0] << 0)  |    \
                   (((uint8_t const *)(p))[1] << 8)  |    \
                   (((uint8_t const *)(p))[2] << 16) |    \
                   (((uint8_t const *)(p))[3] << 24))

#define PST_LE_GET_INT32(p) \
        (int32_t)((((uint8_t const *)(p))[0] << 0)  |    \
                  (((uint8_t const *)(p))[1] << 8)  |    \
                  (((uint8_t const *)(p))[2] << 16) |    \
                  (((uint8_t const *)(p))[3] << 24))

#define PST_LE_GET_UINT16(p)				  \
        (uint16_t)((((uint8_t const *)(p))[0] << 0)  |    \
                   (((uint8_t const *)(p))[1] << 8))

#define PST_LE_GET_INT16(p)				  \
        (int16_t)((((uint8_t const *)(p))[0] << 0)  |    \
                   (((uint8_t const *)(p))[1] << 8))

#define PST_LE_GET_UINT8(p) (*(uint8_t const *)(p))

#define PST_LE_GET_INT8(p) (*(int8_t const *)(p))


#endif //DEFINEH_H
