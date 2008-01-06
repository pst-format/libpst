#ifndef LZFU_H
#define LZFU_H

#include <stdint.h>

unsigned char* lzfu_decompress (unsigned char* rtfcomp, uint32_t compsize, size_t *size);

#endif
