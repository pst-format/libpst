/***
 * define.h
 * Part of the LibPST project
 * Written by David Smith
 *            dave.s@earthcorp.com
 */

#ifndef DEFINEH_H
#define DEFINEH_H

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#include "libpst.h"
#include "timeconv.h"
#include "libstrfunc.h"
#include "vbuf.h"


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
    #include <process.h>
    #undef gmtime_r
    #define gmtime_r(tp,tmp) (gmtime(tp)?(*(tmp)=*gmtime(tp),(tmp)):0)
    #define ctime_r(tp,tmp) (ctime(tp)?(strcpy((tmp),ctime((tp))),(tmp)):0)
#else
    #ifdef __DJGPP__
        #define gmtime_r(tp,tmp) (gmtime(tp)?(*(tmp)=*gmtime(tp),(tmp)):0)
        #define ctime_r(tp,tmp) (ctime(tp)?(strcpy((tmp),ctime((tp))),(tmp)):0)
		#define fseeko(stream, offset, whence) fseek(stream, (long)offset, whence)
        #define ftello ftell
    #endif
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

#ifdef HAVE_SYS_SHM_H
    #include <sys/shm.h>
#endif

#ifdef HAVE_SYS_WAIT_H
    #include <sys/wait.h>
#endif

#ifdef HAVE_DIRENT_H
    #include <dirent.h>
#endif

#ifdef HAVE_SEMAPHORE_H
    #include <semaphore.h>
#endif


void  pst_debug_lock();
void  pst_debug_unlock();
void  pst_debug_init(const char* fname, void* output_mutex);
void  pst_debug_func(const char* function);
void  pst_debug_func_ret();
void  pst_debug(int line, const char *file, const char *fmt, ...);
void  pst_debug_hexdump(int line, const char *file, const char* buf, size_t size, int cols, int delta);
void  pst_debug_hexdumper(FILE* out, const char* buf, size_t size, int cols, int delta);
void  pst_debug_close(void);
void* pst_malloc(size_t size);

#define MESSAGEPRINT(...) pst_debug(__LINE__, __FILE__,  __VA_ARGS__)

#define WARN(x) {           \
    MESSAGEPRINT x;         \
    pst_debug_lock();       \
        printf x;           \
        fflush(stdout);     \
    pst_debug_unlock();     \
}

#define DIE(x) {            \
    WARN(x);                \
    exit(EXIT_FAILURE);     \
}

#define DEBUG_WARN(x)           MESSAGEPRINT x
#define DEBUG_INFO(x)           MESSAGEPRINT x
#define DEBUG_HEXDUMP(x, s)     pst_debug_hexdump(__LINE__, __FILE__, (char*)x, s, 0x10, 0)
#define DEBUG_HEXDUMPC(x, s, c) pst_debug_hexdump(__LINE__, __FILE__, (char*)x, s, c, 0)


#define DEBUG_ENT(x)                                            \
    {                                                           \
        pst_debug_func(x);                                      \
        pst_debug(__LINE__, __FILE__, "Entering function\n");   \
    }
#define DEBUG_RET()                                             \
    {                                                           \
        pst_debug(__LINE__, __FILE__, "Leaving function\n");    \
        pst_debug_func_ret();                                   \
    }

#define DEBUG_INIT(fname,mutex) {pst_debug_init(fname,mutex);}
#define DEBUG_CLOSE()           {pst_debug_close();}
#define RET_DERROR(res, ret_val, x) if (res) { DIE(x);}



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
