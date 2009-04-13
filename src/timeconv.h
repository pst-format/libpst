#ifndef __PST_TIMECONV_H
#define __PST_TIMECONV_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif
    char * pst_fileTimeToAscii (const FILETIME *filetime);
    struct tm * pst_fileTimeToStructTM (const FILETIME *filetime);
    time_t pst_fileTimeToUnixTime( const FILETIME *filetime);
#ifdef __cplusplus
}
#endif

#endif
