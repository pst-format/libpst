#ifndef LZFU_H
#define LZFU_H

#ifdef _MSC_VER
#define u_int32_t unsigned int
#else
#include <stdint.h>
#endif

unsigned char* lzfu_decompress (unsigned char* rtfcomp, u_int32_t compsize, size_t *size);

#endif
