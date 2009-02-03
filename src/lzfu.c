 /*
	 This program is free software; you can redistribute it and/or modify
	 it under the terms of the GNU General Public License as published by
	 the Free Software Foundation; either version 2 of the License, or
	 (at your option) any later version.

	 You should have received a copy of the GNU General Public License
	 along with this program; if not, write to the Free Software Foundation,
	 Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA
  */

#include "common.h"
#include "libpst.h"

#include "define.h"
#include "lzfu.h"

#ifdef HAVE_SYS_TYPES_H
    #include <sys/types.h>
#endif


#define LZFU_COMPRESSED 		0x75465a4c
#define LZFU_UNCOMPRESSED		0x414c454d

// initital dictionary
#define LZFU_INITDICT	"{\\rtf1\\ansi\\mac\\deff0\\deftab720{\\fonttbl;}" \
						"{\\f0\\fnil \\froman \\fswiss \\fmodern \\fscrip" \
						"t \\fdecor MS Sans SerifSymbolArialTimes Ne" \
						"w RomanCourier{\\colortbl\\red0\\green0\\blue0" \
						"\r\n\\par \\pard\\plain\\f0\\fs20\\b\\i\\u\\tab" \
						"\\tx"
// initial length of dictionary
#define LZFU_INITLENGTH 207

// header for compressed rtf
typedef struct _lzfuheader {
	uint32_t cbSize;
	uint32_t cbRawSize;
	uint32_t dwMagic;
	uint32_t dwCRC;
} lzfuheader;


char* lzfu_decompress(char* rtfcomp, uint32_t compsize, size_t *size) {
	unsigned char dict[4096];       // the dictionary buffer
	unsigned int dict_length = 0;   // the dictionary pointer
	lzfuheader lzfuhdr;             // the header of the lzfu block
	unsigned char flags;            // 8 bits of flags (1=2byte block pointer into the dict, 0=1 byte literal)
	unsigned char flag_mask;        // look at one flag bit each time thru the loop
	uint32_t i;
	char    *out_buf;
	uint32_t out_ptr  = 0;
	uint32_t out_size;
	uint32_t in_ptr;
	uint32_t in_size;

	memcpy(dict, LZFU_INITDICT, LZFU_INITLENGTH);
    memset(dict + LZFU_INITLENGTH, 0, sizeof(dict) - LZFU_INITLENGTH);
	dict_length = LZFU_INITLENGTH;

	memcpy(&lzfuhdr, rtfcomp, sizeof(lzfuhdr));
	LE32_CPU(lzfuhdr.cbSize);
	LE32_CPU(lzfuhdr.cbRawSize);
	LE32_CPU(lzfuhdr.dwMagic);
	LE32_CPU(lzfuhdr.dwCRC);
	//printf("total size: %d\n", lzfuhdr.cbSize+4);
	//printf("raw size  : %d\n", lzfuhdr.cbRawSize);
	//printf("compressed: %s\n", (lzfuhdr.dwMagic == LZFU_COMPRESSED ? "yes" : "no"));
	//printf("CRC       : %#x\n", lzfuhdr.dwCRC);
	//printf("\n");
	out_size = lzfuhdr.cbRawSize;
	out_buf  = (char*)xmalloc(out_size);
	in_ptr	 = sizeof(lzfuhdr);
	// Make sure to correct lzfuhdr.cbSize with 4 bytes before comparing
	// to compsize
	in_size  = (lzfuhdr.cbSize + 4 < compsize) ? lzfuhdr.cbSize + 4 : compsize;
	while (in_ptr < in_size) {
		flags = (unsigned char)(rtfcomp[in_ptr++]);
		flag_mask = 1;
		while (flag_mask) {
			if (flag_mask & flags) {
				// two bytes available?
				if (in_ptr+1 < in_size) {
					// read 2 bytes from input
					unsigned short int blkhdr, offset, length;
					memcpy(&blkhdr, rtfcomp+in_ptr, 2);
					LE16_CPU(blkhdr);
					in_ptr += 2;
					/* swap the upper and lower bytes of blkhdr */
					blkhdr = (((blkhdr&0xFF00)>>8)+
							  ((blkhdr&0x00FF)<<8));
					/* the offset is the first 12 bits of the 16 bit value */
					offset = (blkhdr&0xFFF0)>>4;
					/* the length of the dict entry are the last 4 bits */
					length = (blkhdr&0x000F)+2;
					// add the value we are about to print to the dictionary
					for (i=0; i < length; i++) {
						unsigned char c1;
						c1 = dict[(offset+i)%4096];
						dict[dict_length] = c1;
						dict_length = (dict_length+1) % 4096;
						if (out_ptr < out_size) out_buf[out_ptr++] = (char)c1;
						// required for dictionary wrap around
						// otherwise 0 byte values are referenced incorrectly
						dict[dict_length] = 0;
					}
				}
			} else {
				// one byte available?
				if (in_ptr < in_size) {
					// uncompressed chunk (single byte)
					char c1 = rtfcomp[in_ptr++];
					dict[dict_length] = c1;
					dict_length = (dict_length+1)%4096;
					if (out_ptr < out_size) out_buf[out_ptr++] = (char)c1;
					// required for dictionary wrap around
					// otherwise 0 byte values are referenced incorrect
					dict[dict_length] = 0;
				}
			}
			flag_mask <<= 1;
		}
	}
    *size = out_ptr;
	return out_buf;
}
