 /*
     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published by
     the Free Software Foundation; either version 2 of the License, or
     (at your option) any later version.

     You should have received a copy of the GNU General Public License
     along with this program; if not, write to the Free Software Foundation,
     Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
  */

#include "define.h"
#include "libpst.h"
#include <sys/types.h>
#include <string.h>
#include <stdio.h>

#ifndef _MSC_VER
#include <stdint.h>
#endif

#ifdef _MSC_VER
#define uint32_t unsigned int
#endif

#define LZFU_COMPRESSED         0x75465a4c
#define LZFU_UNCOMPRESSED       0x414c454d

// initital dictionary
#define LZFU_INITDICT   "{\\rtf1\\ansi\\mac\\deff0\\deftab720{\\fonttbl;}" \
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


/** 
    We always need to add 0x10 to the buffer offset because we need to skip past the header info
*/

unsigned char* lzfu_decompress (unsigned char* rtfcomp) {
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
  unsigned int i, in_size;
  unsigned char *out_buf;
  unsigned int out_ptr = 0;

  memcpy(dict, LZFU_INITDICT, LZFU_INITLENGTH);
  dict_length = LZFU_INITLENGTH;
  memcpy(&lzfuhdr, rtfcomp, sizeof(lzfuhdr));
  LE32_CPU(lzfuhdr.cbSize);   LE32_CPU(lzfuhdr.cbRawSize);
  LE32_CPU(lzfuhdr.dwMagic);  LE32_CPU(lzfuhdr.dwCRC);
  /*  printf("total size: %d\n", lzfuhdr.cbSize+4);
  printf("raw size  : %d\n", lzfuhdr.cbRawSize);
  printf("compressed: %s\n", (lzfuhdr.dwMagic == LZFU_COMPRESSED ? "yes" : "no"));
  printf("CRC       : %#x\n", lzfuhdr.dwCRC);
  printf("\n");*/
  out_buf = (unsigned char*)xmalloc(lzfuhdr.cbRawSize+20); //plus 4 cause we have 2x'}' and a \0
  in_size = 0;
  // we add plus one here cause when referencing an array, the index is always one less 
  // (ie, when accessing 2 element array, highest index is [1])
  while (in_size+0x11 < lzfuhdr.cbSize) {
    memcpy(&flags, &(rtfcomp[in_size+0x10]), 1);
    in_size += 1;

    flag_mask = 1;
    while (flag_mask != 0 && in_size+0x11 < lzfuhdr.cbSize) {
      if (flag_mask & flags) {
	// read 2 bytes from input
	unsigned short int blkhdr, offset, length;
	memcpy(&blkhdr, &(rtfcomp[in_size+0x10]), 2);
	LE16_CPU(blkhdr);
	in_size += 2;
	/* swap the upper and lower bytes of blkhdr */
	blkhdr = (((blkhdr&0xFF00)>>8)+
		  ((blkhdr&0x00FF)<<8));
	/* the offset is the first 24 bits of the 32 bit value */
	offset = (blkhdr&0xFFF0)>>4;
	/* the length of the dict entry are the last 8 bits */
	length = (blkhdr&0x000F)+2;
	// add the value we are about to print to the dictionary
	for (i=0; i < length; i++) {
	  unsigned char c1;
	  c1 = dict[(offset+i)%4096];
	  dict[dict_length]=c1;
	  dict_length = (dict_length+1) % 4096;
	  out_buf[out_ptr++] = c1;
	}
      } else {
	// uncompressed chunk (single byte)
	char c1 = rtfcomp[in_size+0x10];
	in_size ++;
	dict[dict_length] = c1;
	dict_length = (dict_length+1)%4096;
	out_buf[out_ptr++] = c1;
      }
      flag_mask <<= 1;
    }
  }
  // the compressed version doesn't appear to drop the closing braces onto the doc.
  // we should do that
  out_buf[out_ptr++] = '}';
  out_buf[out_ptr++] = '}';
  out_buf[out_ptr++] = '\0';
  return out_buf;
}
