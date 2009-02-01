
#ifndef __COMMON_H
#define __COMMON_H

#ifndef _WIN32
    typedef uint32_t  DWORD;
    typedef uint16_t   WORD;
    typedef uint8_t    BYTE;
    typedef uint32_t UINT32;

# pragma pack (1)

#ifndef FILETIME_DEFINED
    #define FILETIME_DEFINED
    /*Win32 Filetime struct - copied from WINE*/
    typedef struct {
        DWORD  dwLowDateTime;
        DWORD  dwHighDateTime;
    } FILETIME;
#endif // FILETIME_DEFINED

#endif // _WIN32
#endif // __COMMON_H
