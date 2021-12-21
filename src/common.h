
#ifndef __PST_COMMON_H
#define __PST_COMMON_H


#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>


#if !defined(_MSC_VER) || (_MSC_VER >= 1800)
    #include <stdint.h>
    #include <inttypes.h>
#else
    typedef signed char        int8_t;
    typedef unsigned char      uint8_t;
    typedef unsigned short     uint16_t;
    typedef short              int16_t;
    typedef unsigned int       uint32_t;
    typedef int                int32_t;
    typedef unsigned long long uint64_t;
    typedef long long          int64_t;
#endif

#ifndef _WIN32
    typedef struct {
        uint32_t  dwLowDateTime;
        uint32_t  dwHighDateTime;
    } FILETIME;
    // According to Jan Wolter, sys/param.h is the most portable source of endian
    // information on UNIX systems. see https://www.unixpapa.com/incnote/byteorder.html
    #include <sys/param.h>
#else
    #include <windows.h>
#endif

#if defined(_WIN32) || defined(__DJGPP__)
    #ifndef LITTLE_ENDIAN
        #define LITTLE_ENDIAN 1234
    #endif
    #ifndef BIG_ENDIAN
        #define BIG_ENDIAN 4321
    #endif
    #define BYTE_ORDER LITTLE_ENDIAN
#endif


#endif

#if defined (__SVR4) && defined (__sun)
    #define LITTLE_ENDIAN 1234
    #define BYTE_ORDER LITTLE_ENDIAN
#endif
