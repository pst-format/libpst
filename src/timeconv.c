#include "define.h"



char* pst_fileTimeToAscii(const FILETIME* filetime, char* result) {
    time_t t;
    t = pst_fileTimeToUnixTime(filetime);
    return ctime_r(&t, result);
}


void pst_fileTimeToStructTM (const FILETIME *filetime, struct tm *result) {
    time_t t1;
    t1 = pst_fileTimeToUnixTime(filetime);
    gmtime_r(&t1, result);
}


time_t pst_fileTimeToUnixTime(const FILETIME *filetime)
{
    uint64_t t = filetime->dwHighDateTime;
    const uint64_t bias = 11644473600LL;
    t <<= 32;
    t += filetime->dwLowDateTime;
    t /= 10000000;
    t -= bias;
    return ((t > (uint64_t)0x000000007fffffff) && (sizeof(time_t) <= 4)) ? 0 : (time_t)t;
}

