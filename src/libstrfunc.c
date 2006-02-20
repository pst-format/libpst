
/* Taken from LibStrfunc v7.3 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "libstrfunc.h"

char *_sf_b64_buf=NULL;
size_t _sf_b64_len=0;


static unsigned char _sf_uc_ib[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/==";

char *
base64_encode(void *data, size_t size) {
  char *output;
  register char *ou;
  register unsigned char *p=(unsigned char *)data;
#ifdef __LINUX__
  register void * dte = ((char*)data + size);
#endif

#ifndef  __LINUX__
  register void * dte = (void*)((char*)data + size);
#endif
  //register void *dte=data + size;
  register int nc=0;
  
  if(data == NULL)
    return NULL;
  
  ou=output=(char *)malloc(size / 3 * 4 + (size / 50) + 5);
  if(!output)
    return NULL;
  
  while((char *)dte - (char *)p >= 3) {
    *ou = _sf_uc_ib[ *p >> 2 ];
    ou[1] = _sf_uc_ib[ ((*p & 0x03) << 4) | (p[1] >> 4) ];
    ou[2] = _sf_uc_ib[ ((p[1] & 0x0F) << 2) | (p[2] >> 6) ];
    ou[3] = _sf_uc_ib[ p[2] & 0x3F ];
    
    p+=3;
    ou+=4;
    
    nc+=4;
    if(!(nc % 76)) *ou++='\n';
  };
  if((char *)dte - (char *)p == 2) {
    *ou++ = _sf_uc_ib[ *p >> 2 ];
    *ou++ = _sf_uc_ib[ ((*p & 0x03) << 4) | (p[1] >> 4) ];
    *ou++ = _sf_uc_ib[ ((p[1] & 0x0F) << 2) ];
    *ou++ = '=';
  } else if((char *)dte - (char *)p == 1) {
    *ou++ = _sf_uc_ib[ *p >> 2 ];
    *ou++ = _sf_uc_ib[ ((*p & 0x03) << 4) ];
    *ou++ = '=';
    *ou++ = '=';
  };
  
  *ou=0;
  
  _sf_b64_len = (ou - output);
  
  if(_sf_b64_buf)
    free(_sf_b64_buf);
  return _sf_b64_buf=output;
};

