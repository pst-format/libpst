
#ifndef __COMMON_H
#define __COMMON_H

#ifndef _WIN32
  typedef unsigned int DWORD;
  typedef unsigned short int WORD;
  typedef unsigned char BYTE;
  typedef unsigned int UINT32;

# pragma pack (1)

# ifndef FILETIME_DEFINED
# define FILETIME_DEFINED
  /*Win32 Filetime struct - copied from WINE*/
  typedef struct {
    unsigned int dwLowDateTime;
    unsigned int dwHighDateTime;
  } FILETIME;
# endif
#endif // _WIN32
#endif // __COMMON_H
