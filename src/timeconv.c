#include "define.h"



char * pst_fileTimeToAscii(const FILETIME* filetime) {
    time_t t;
    t = pst_fileTimeToUnixTime(filetime);
    return ctime(&t);
}


struct tm * pst_fileTimeToStructTM (const FILETIME *filetime) {
    time_t t1;
    t1 = pst_fileTimeToUnixTime(filetime);
    return gmtime(&t1);
}


time_t pst_fileTimeToUnixTime(const FILETIME *filetime)
{
    int64_t t = filetime->dwHighDateTime;
    t <<= 32;
    t += filetime->dwLowDateTime;
    t -= 116444736000000000LL;
    if (t < 0) {
        return -1 - ((-t - 1) / 10000000);
    }
    else {
        return t / 10000000;
    }
}

