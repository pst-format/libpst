/***
 * define.h
 * Part of the LibPST project
 * Written by David Smith
 *            dave.s@earthcorp.com
 */

#ifdef HAVE_CONFIG_H
    #include "config.h"
#else
    #ifdef _MSC_VER
        #undef  HAVE_UNISTD_H
        #define HAVE_DIRECT_H
        #define HAVE_WINDOWS_H
    #endif
#endif
#include "version.h"

#ifndef DEFINEH_H
#define DEFINEH_H

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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <wchar.h>
#include <signal.h>
#include <errno.h>

#define PERM_DIRS 0777

#ifdef HAVE_UNISTD_H
    #include <unistd.h>
    #define D_MKDIR(x) mkdir(x, PERM_DIRS)
#else
    #include "XGetopt.h"
    #ifdef HAVE_DIRECT_H
        #include <direct.h>    // win32
        #define D_MKDIR(x) mkdir(x)
        #define chdir      _chdir
    #endif

    #ifdef HAVE_WINDOWS_H
        #include <windows.h>
    #endif

    #ifdef _MSC_VER
        #define vsnprintf  _vsnprintf
        #define snprintf   _snprintf
        #define ftello     _ftelli64
        #define fseeko     _fseeki64
        #define strcasecmp _stricmp
        #define off_t      __int64
        #define size_t     __int64
        #define int64_t    __int64
        #define uint64_t   unsigned __int64
        #define int32_t    __int32
        #define uint32_t   unsigned int
        #define int16_t    short int
        #define uint16_t   unsigned short int
        #define int8_t     signed char
        #define uint8_t    unsigned char
        #define UINT64_MAX ((uint64_t)0xffffffffffffffff)
        int __cdecl _fseeki64(FILE *, __int64, int);
        __int64 __cdecl _ftelli64(FILE *);
    #endif
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

#endif //DEFINEH_H
