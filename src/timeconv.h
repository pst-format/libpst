#ifndef __TIMECONV_H
#define __TIMECONV_H

#include "common.h"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif
  time_t fileTimeToUnixTime( const FILETIME *filetime, DWORD *remainder );

  char * fileTimeToAscii (const FILETIME *filetime);

  struct tm * fileTimeToStructTM (const FILETIME *filetime);
  
#ifdef __cplusplus
}
#endif

#endif
