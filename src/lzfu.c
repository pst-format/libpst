 /*
	 This program is free software; you can redistribute it and/or modify
	 it under the terms of the GNU General Public License as published by
	 the Free Software Foundation; either version 2 of the License, or
	 (at your option) any later version.

	 You should have received a copy of the GNU General Public License
	 along with this program; if not, write to the Free Software Foundation,
	 Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA
  */

#include "define.h"
#include "libpst.h"
#include <sys/types.h>
#include <string.h>
#include <stdio.h>

#include "lzfu.h"

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
	u_int32_t cbSize;
	u_int32_t cbRawSize;
	u_int32_t dwMagic;
	u_int32_t dwCRC;
} lzfuheader;


unsigned char* lzfu_decompress (unsigned char* rtfcomp, u_int32_t compsize, size_t *size) {
	// the dictionary buffer
	unsigned char dict[4096];
	// the dictionary pointer
	unsigned int dict_length=0;
	// the header of the lzfu block
	lzfuheader lzfuhdr;
	// container for the data blocks
	unsigned char flags;
	// temp value for determining the bits in the flag
	unsigned char flag_mask;
	u_int32_t i;
	unsigned char *out_buf;
	u_int32_t out_ptr  = 0;
	u_int32_t out_size;
	u_int32_t in_ptr;
	u_int32_t in_size;

	memcpy(dict, LZFU_INITDICT, LZFU_INITLENGTH);
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
	out_size = lzfuhdr.cbRawSize + 3;	// two braces and a null terminator
	out_buf  = (unsigned char*)xmalloc(out_size);
	in_ptr	 = sizeof(lzfuhdr);
	in_size  = (lzfuhdr.cbSize < compsize) ? lzfuhdr.cbSize : compsize;
	while (in_ptr < in_size) {
		flags = rtfcomp[in_ptr++];
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
						dict[dict_length]=c1;
						dict_length = (dict_length+1) % 4096;
						if (out_ptr < out_size) out_buf[out_ptr++] = c1;
					}
				}
			} else {
				// one byte available?
				if (in_ptr < in_size) {
					// uncompressed chunk (single byte)
					char c1 = rtfcomp[in_ptr++];
					dict[dict_length] = c1;
					dict_length = (dict_length+1)%4096;
					if (out_ptr < out_size) out_buf[out_ptr++] = c1;
				}
			}
			flag_mask <<= 1;
		}
	}
	// the compressed version doesn't appear to drop the closing
	// braces onto the doc, so we do that here.
	if (out_ptr < out_size) out_buf[out_ptr++] = '}';
	if (out_ptr < out_size) out_buf[out_ptr++] = '}';
	*size = out_ptr;
	if (out_ptr < out_size) out_buf[out_ptr++] = '\0';
	return out_buf;
}
