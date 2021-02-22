
#ifndef __PST_LIBSTRFUNC_H
#define __PST_LIBSTRFUNC_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

char *pst_base64_encode(void *data, size_t size);
char *pst_base64_encode_single(void *data, size_t size);
char *pst_base64_encode_multiple(void *data, size_t size, int *line_count);

#ifdef __cplusplus
}
#endif

#endif

