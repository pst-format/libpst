#ifndef LZFU_H
#define LZFU_H

#include <stdint.h>

char* lzfu_decompress (char* rtfcomp, uint32_t compsize, size_t *size);

#endif
