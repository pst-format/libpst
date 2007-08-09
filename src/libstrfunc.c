
/* Taken from LibStrfunc v7.3 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "libstrfunc.h"


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

  if ( data == NULL || size == 0 )
	return NULL;

  ou=output=(char *)malloc(size / 3 * 4 + (size / 57) + 5);
  if(!output)
	return NULL;

  while((char *)dte - (char *)p >= 3) {
	unsigned char x = p[0];
	unsigned char y = p[1];
	unsigned char z = p[2];
	ou[0] = _sf_uc_ib[ x >> 2 ];
	ou[1] = _sf_uc_ib[ ((x & 0x03) << 4) | (y >> 4) ];
	ou[2] = _sf_uc_ib[ ((y & 0x0F) << 2) | (z >> 6) ];
	ou[3] = _sf_uc_ib[ z & 0x3F ];
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

  return output;
};

