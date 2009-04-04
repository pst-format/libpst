
/* Taken from LibStrfunc v7.3 */

#include "define.h"


static char base64_code_chars[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/==";

static void base64_append(char **ou, int *line_count, char data);
static void base64_append(char **ou, int *line_count, char data)
{
    if (*line_count == 76) {
        *(*ou)++ = '\n';
        *line_count = 0;
    }
    *(*ou)++ = data;
    (*line_count)++;
}


char *pst_base64_encode(void *data, size_t size)
{
    int line_count = 0;
    return pst_base64_encode_multiple(data, size, &line_count);
}


char *pst_base64_encode_multiple(void *data, size_t size, int *line_count)
{
    char *output;
    char *ou;
    unsigned char *p   = (unsigned char *)data;
    unsigned char *dte = p + size;

    if (data == NULL || size == 0) return NULL;

    ou = output = (char *)malloc(size / 3 * 4 + (size / 57) + 6);
    if (!output) return NULL;

    while((dte-p) >= 3) {
        unsigned char x = p[0];
        unsigned char y = p[1];
        unsigned char z = p[2];
        base64_append(&ou, line_count, base64_code_chars[ x >> 2 ]);
        base64_append(&ou, line_count, base64_code_chars[ ((x & 0x03) << 4) | (y >> 4) ]);
        base64_append(&ou, line_count, base64_code_chars[ ((y & 0x0F) << 2) | (z >> 6) ]);
        base64_append(&ou, line_count, base64_code_chars[ z & 0x3F ]);
        p+=3;
    };
    if ((dte-p) == 2) {
        base64_append(&ou, line_count, base64_code_chars[ *p >> 2 ]);
        base64_append(&ou, line_count, base64_code_chars[ ((*p & 0x03) << 4) | (p[1] >> 4) ]);
        base64_append(&ou, line_count, base64_code_chars[ ((p[1] & 0x0F) << 2) ]);
        base64_append(&ou, line_count, '=');
    } else if ((dte-p) == 1) {
        base64_append(&ou, line_count, base64_code_chars[ *p >> 2 ]);
        base64_append(&ou, line_count, base64_code_chars[ ((*p & 0x03) << 4) ]);
        base64_append(&ou, line_count, '=');
        base64_append(&ou, line_count, '=');
    };

    *ou=0;
    return output;
};


