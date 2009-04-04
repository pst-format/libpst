#ifndef __TIMECONV_H
#define __TIMECONV_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif
    time_t pst_fileTimeToUnixTime( const FILETIME *filetime, DWORD *remainder );
    char * pst_fileTimeToAscii (const FILETIME *filetime);
    struct tm * pst_fileTimeToStructTM (const FILETIME *filetime);
#ifdef __cplusplus
}
#endif

#endif
