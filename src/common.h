
#ifndef __COMMON_H
#define __COMMON_H


#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>


#ifndef  _MSC_VER
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
    typedef uint32_t  DWORD;
    typedef uint16_t   WORD;
    typedef uint8_t    BYTE;
    typedef uint32_t UINT32;
    typedef struct {    // copied from wine
        DWORD  dwLowDateTime;
        DWORD  dwHighDateTime;
    } FILETIME;
    // According to Jan Wolter, sys/param.h is the most portable source of endian
    // information on UNIX systems. see http://www.unixpapa.com/incnote/byteorder.html
    #include <sys/param.h>
#else
    #include <windows.h>
    #define BYTE_ORDER LITTLE_ENDIAN
#endif


#endif
