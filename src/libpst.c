/***
 * libpst.c
 * Part of the LibPST project
 * Written by David Smith
 *			  dave.s@earthcorp.com
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <wchar.h>

#include <errno.h>
#include <sys/stat.h>	// mkdir
#include <fcntl.h>		// for Win32 definition of _O_BINARY
#include "define.h"
#include "libstrfunc.h"

#ifdef _MSC_VER
# include <windows.h>
#else
# include <unistd.h>
#endif //ifdef _MSC_VER

#include "libpst.h"
#include "timeconv.h"

#define FILE_SIZE_POINTER 0xA8
#define INDEX_POINTER	  0xC4
#define SECOND_POINTER	  0xBC
// the encryption setting could be at 0x1CC. Will require field testing
#define ENC_OFFSET		  0x1CD
// says the type of index we have
#define INDEX_TYPE_OFFSET 0x0A

// for the 64bit 2003 outlook PST we need new file offsets
// perhaps someone can figure out the header format for the pst files...
#define FILE_SIZE_POINTER_64 0xB8
#define INDEX_POINTER_64	 0xF0
#define SECOND_POINTER_64	 0xE0

#define PST_SIGNATURE 0x4E444221

struct _pst_table_ptr_struct{
	int32_t start;
	int32_t u1;
	int32_t offset;
};

typedef struct _pst_block_header {
	int16_t type;
	int16_t count;
} pst_block_header;

typedef struct _pst_id2_assoc {
	u_int32_t id2;
	u_int32_t id;
	int32_t table2;
} pst_id2_assoc;

// this is an array of the un-encrypted values. the un-encrypted value is in the position
// of the encrypted value. ie the encrypted value 0x13 represents 0x02
//					   0	 1	   2	 3	   4	 5	   6	 7
//					   8	 9	   a	 b	   c	 d	   e	 f
unsigned char comp_enc [] =
  { 0x47, 0xf1, 0xb4, 0xe6, 0x0b, 0x6a, 0x72, 0x48,
	0x85, 0x4e, 0x9e, 0xeb, 0xe2, 0xf8, 0x94, 0x53, /*0x0f*/
	0xe0, 0xbb, 0xa0, 0x02, 0xe8, 0x5a, 0x09, 0xab,
	0xdb, 0xe3, 0xba, 0xc6, 0x7c, 0xc3, 0x10, 0xdd, /*0x1f*/
	0x39, 0x05, 0x96, 0x30, 0xf5, 0x37, 0x60, 0x82,
	0x8c, 0xc9, 0x13, 0x4a, 0x6b, 0x1d, 0xf3, 0xfb, /*0x2f*/
	0x8f, 0x26, 0x97, 0xca, 0x91, 0x17, 0x01, 0xc4,
	0x32, 0x2d, 0x6e, 0x31, 0x95, 0xff, 0xd9, 0x23, /*0x3f*/
	0xd1, 0x00, 0x5e, 0x79, 0xdc, 0x44, 0x3b, 0x1a,
	0x28, 0xc5, 0x61, 0x57, 0x20, 0x90, 0x3d, 0x83, /*0x4f*/
	0xb9, 0x43, 0xbe, 0x67, 0xd2, 0x46, 0x42, 0x76,
	0xc0, 0x6d, 0x5b, 0x7e, 0xb2, 0x0f, 0x16, 0x29, /*0x5f*/
	0x3c, 0xa9, 0x03, 0x54, 0x0d, 0xda, 0x5d, 0xdf,
	0xf6, 0xb7, 0xc7, 0x62, 0xcd, 0x8d, 0x06, 0xd3, /*0x6f*/
	0x69, 0x5c, 0x86, 0xd6, 0x14, 0xf7, 0xa5, 0x66,
	0x75, 0xac, 0xb1, 0xe9, 0x45, 0x21, 0x70, 0x0c, /*0x7f*/
	0x87, 0x9f, 0x74, 0xa4, 0x22, 0x4c, 0x6f, 0xbf,
	0x1f, 0x56, 0xaa, 0x2e, 0xb3, 0x78, 0x33, 0x50, /*0x8f*/
	0xb0, 0xa3, 0x92, 0xbc, 0xcf, 0x19, 0x1c, 0xa7,
	0x63, 0xcb, 0x1e, 0x4d, 0x3e, 0x4b, 0x1b, 0x9b, /*0x9f*/
	0x4f, 0xe7, 0xf0, 0xee, 0xad, 0x3a, 0xb5, 0x59,
	0x04, 0xea, 0x40, 0x55, 0x25, 0x51, 0xe5, 0x7a, /*0xaf*/
	0x89, 0x38, 0x68, 0x52, 0x7b, 0xfc, 0x27, 0xae,
	0xd7, 0xbd, 0xfa, 0x07, 0xf4, 0xcc, 0x8e, 0x5f, /*0xbf*/
	0xef, 0x35, 0x9c, 0x84, 0x2b, 0x15, 0xd5, 0x77,
	0x34, 0x49, 0xb6, 0x12, 0x0a, 0x7f, 0x71, 0x88, /*0xcf*/
	0xfd, 0x9d, 0x18, 0x41, 0x7d, 0x93, 0xd8, 0x58,
	0x2c, 0xce, 0xfe, 0x24, 0xaf, 0xde, 0xb8, 0x36, /*0xdf*/
	0xc8, 0xa1, 0x80, 0xa6, 0x99, 0x98, 0xa8, 0x2f,
	0x0e, 0x81, 0x65, 0x73, 0xe4, 0xc2, 0xa2, 0x8a, /*0xef*/
	0xd4, 0xe1, 0x11, 0xd0, 0x08, 0x8b, 0x2a, 0xf2,
	0xed, 0x9a, 0x64, 0x3f, 0xc1, 0x6c, 0xf9, 0xec}; /*0xff*/

int32_t pst_open(pst_file *pf, char *name, char *mode) {
	u_int32_t sig;

	DEBUG_ENT("pst_open");

	if (!pf) {
		WARN (("cannot be passed a NULL pst_file\n"));
		DEBUG_RET();
		return -1;
	}
	memset(pf, 0, sizeof(pst_file));

#ifdef _MSC_VER
	// set the default open mode for windows
	_fmode = _O_BINARY;
#endif //_MSC_VER
	if ((pf->fp = fopen(name, mode)) == NULL) {
		WARN(("cannot open PST file. Error\n"));
		DEBUG_RET();
		return -1;
	}

	// Check pst file magic
	if (fread(&sig, sizeof(sig), 1, pf->fp) == 0) {
		fclose(pf->fp);
		WARN(("cannot read signature from PST file. Closing on error\n"));
		DEBUG_RET();
		return -1;
	}
	LE32_CPU(sig);	// architecture independant byte-swapping (little, big, pdp)
	DEBUG_INFO(("sig = %X\n", sig));
	if (sig != PST_SIGNATURE) {
		fclose(pf->fp);
		WARN(("not a PST file that I know. Closing with error\n"));
		DEBUG_RET();
		return -1;
	}

	// read index type
	_pst_getAtPos(pf->fp, INDEX_TYPE_OFFSET, &(pf->ind_type), sizeof(unsigned char));
	DEBUG_INFO(("index_type = %i\n", pf->ind_type));
	if (pf->ind_type != 0x0E) {
		WARN(("unknown index structure. Could this be a new Outlook 2003 PST file?\n"));
		DEBUG_RET();
		return -1;
	}

	// read encryption setting
	_pst_getAtPos(pf->fp, ENC_OFFSET, &(pf->encryption), sizeof(unsigned char));
	DEBUG_INFO(("encrypt = %i\n", pf->encryption));

	_pst_getAtPos(pf->fp, SECOND_POINTER-4, &(pf->index2_count), sizeof(pf->index2_count));
	_pst_getAtPos(pf->fp, SECOND_POINTER, &(pf->index2), sizeof(pf->index2));
	LE32_CPU(pf->index2_count);
	LE32_CPU(pf->index2);
	DEBUG_INFO(("Pointer2 is %#X, count %i[%#x]\n", pf->index2, pf->index2_count, pf->index2_count));

	_pst_getAtPos(pf->fp, FILE_SIZE_POINTER, &(pf->size), sizeof(pf->size));
	LE32_CPU(pf->size);

	_pst_getAtPos(pf->fp, INDEX_POINTER-4, &(pf->index1_count), sizeof(pf->index1_count));
	_pst_getAtPos(pf->fp, INDEX_POINTER, &(pf->index1), sizeof(pf->index1));
	LE32_CPU(pf->index1_count);
	LE32_CPU(pf->index1);
	DEBUG_INFO(("Pointer1 is %#X, count %i[%#x]\n", pf->index1, pf->index1_count, pf->index1_count));

	DEBUG_RET();
	return 0;
}


int32_t pst_close(pst_file *pf) {
	DEBUG_ENT("pst_close");
	if (!pf->fp) {
		WARN(("cannot close NULL fp\n"));
		DEBUG_RET();
		return -1;
	}
	if (fclose(pf->fp)) {
		WARN(("fclose returned non-zero value\n"));
		DEBUG_RET();
		return -1;
	}
	// we must free the id linklist and the desc tree
	_pst_free_id (pf->i_head);
	_pst_free_desc (pf->d_head);
	_pst_free_xattrib (pf->x_head);
	DEBUG_RET();
	return 0;
}


pst_desc_ll* pst_getTopOfFolders(pst_file *pf, pst_item *root) {
	pst_desc_ll *ret;
	DEBUG_ENT("pst_getTopOfFolders");
	if (!root || !root->message_store) {
		DEBUG_INDEX(("There isn't a top of folder record here.\n"));
		ret = NULL;
	} else if (!root->message_store->top_of_personal_folder) {
		// this is the OST way
		// ASSUMPTION: Top Of Folders record in PST files is *always* descid 0x2142
		ret = _pst_getDptr(pf, 0x2142);
	} else {
		ret = _pst_getDptr(pf, root->message_store->top_of_personal_folder->id);
	}
	DEBUG_RET();
	return ret;
}


int32_t pst_attach_to_mem(pst_file *pf, pst_item_attach *attach, unsigned char **b){
	int32_t size=0;
	pst_index_ll *ptr;
	struct holder h = {b, NULL, 0, "", 0};
	DEBUG_ENT("pst_attach_to_mem");
	if (attach->id_val != -1) {
		ptr = _pst_getID(pf, attach->id_val);
		if (ptr) {
			size = _pst_ff_getID2data(pf, ptr, &h);
		} else {
			DEBUG_WARN(("Couldn't find ID pointer. Cannot handle attachment\n"));
			size = 0;
		}
		attach->size = size; // may as well update it to what is correct for this instance
	} else {
		size = attach->size;
	}
	DEBUG_RET();
	return size;
}


int32_t pst_attach_to_file(pst_file *pf, pst_item_attach *attach, FILE* fp) {
	pst_index_ll *ptr;
	struct holder h = {NULL, fp, 0, "", 0};
	int32_t size;
	DEBUG_ENT("pst_attach_to_file");
	if (attach->id_val != -1) {
		ptr = _pst_getID(pf, attach->id_val);
		if (ptr) {
			size = _pst_ff_getID2data(pf, ptr, &h);
		} else {
			DEBUG_WARN(("Couldn't find ID pointer. Cannot save attachment to file\n"));
			size = 0;
		}
		attach->size = size;
	} else {
		// save the attachment to file
		size = attach->size;
		pst_fwrite(attach->data, 1, size, fp);
	}
	DEBUG_RET();
	return 1;
}


int32_t pst_attach_to_file_base64(pst_file *pf, pst_item_attach *attach, FILE* fp) {
	pst_index_ll *ptr;
	struct holder h = {NULL, fp, 1, "", 0};
	int32_t size;
	char *c;
	DEBUG_ENT("pst_attach_to_file_base64");
	if (attach->id_val != -1) {
		ptr = _pst_getID(pf, attach->id_val);
		if (ptr) {
			size = _pst_ff_getID2data(pf, ptr, &h);
			// will need to encode any bytes left over
			c = base64_encode(h.base64_extra_chars, h.base64_extra);
			if (c) {
				pst_fwrite(c, 1, strlen(c), fp);
				free(c);	// caught by valgrind
			}
		} else {
			DEBUG_WARN (("Couldn't find ID pointer. Cannot save attachement to Base64\n"));
			size = 0;
		}
		attach->size = size;
	} else {
		// encode the attachment to the file
		c = base64_encode(attach->data, attach->size);
		if (c) {
			pst_fwrite(c, 1, strlen(c), fp);
			free(c);	// caught by valgrind
		}
		size = attach->size;
	}
	DEBUG_RET();
	return 1;
}


int32_t pst_load_index (pst_file *pf) {
	int32_t   x;
	u_int32_t y;
	DEBUG_ENT("pst_load_index");
	if (!pf) {
		WARN(("Cannot load index for a NULL pst_file\n"));
		DEBUG_RET();
		return -1;
	}

	x = _pst_build_id_ptr(pf, pf->index1, 0, pf->index1_count, 0, INT32_MAX);
	DEBUG_INDEX(("build id ptr returns %i\n", x));

	y = 0;
	x = _pst_build_desc_ptr(pf, pf->index2, 0, pf->index2_count, &y, 0x21, INT32_MAX);
	DEBUG_INDEX(("build desc ptr returns %i\n", x));

	DEBUG_CODE(_pst_printDptr(pf););
	DEBUG_RET();
	return 0;
}


pst_desc_ll* pst_getNextDptr(pst_desc_ll* d) {
	pst_desc_ll* r = NULL;
	DEBUG_ENT("pst_getNextDptr");
	if (d) {
		if ((r = d->child) == NULL) {
			while (!d->next && d->parent) d = d->parent;
			r = d->next;
		}
	}
	DEBUG_RET();
	return r;
}


typedef struct _pst_x_attrib {
	u_int16_t extended;
	u_int16_t zero;
	u_int16_t type;
	u_int16_t map;
} pst_x_attrib;


int32_t pst_load_extended_attributes(pst_file *pf) {
	// for PST files this will load up ID2 0x61 and check it's "list" attribute.
	pst_desc_ll *p;
	pst_num_array *na;
	pst_index2_ll *list2;
	unsigned char * buffer=NULL, *headerbuffer=NULL;//, *tc;
	pst_x_attrib xattrib;
	int32_t bptr = 0, bsize, hsize, tint, err=0, x;
	pst_x_attrib_ll *ptr, *p_head=NULL, *p_sh=NULL, *p_sh2=NULL;

	DEBUG_ENT("pst_loadExtendedAttributes");
	if ((p = _pst_getDptr(pf, 0x61)) == NULL) {
		DEBUG_WARN(("Cannot find DescID 0x61 for loading the Extended Attributes\n"));
		DEBUG_RET();
		return 0;
	}
	if (p->list_index) {
		list2 = _pst_build_id2(pf, p->list_index, NULL);
	}
	if (!p->desc) {
		DEBUG_WARN(("desc is NULL for item 0x61. Cannot load Extended Attributes\n"));
		DEBUG_RET();
		return 0;
	}
	if ((na = _pst_parse_block(pf, p->desc->id, list2)) == NULL) {
		DEBUG_WARN(("Cannot process desc block for item 0x61. Not loading extended Attributes\n"));
		DEBUG_RET();
		return 0;
	}
	x = 0;
	while (x < na->count_item) {
		if (na->items[x]->id == 0x0003) {
			buffer = na->items[x]->data;
			bsize = na->items[x]->size;
		} else if (na->items[x]->id == 0x0004) {
			headerbuffer = na->items[x]->data;
			hsize = na->items[x]->size;
		}
		x++;
	}

	if (!buffer) {
		if (na) _pst_free_list(na);
		DEBUG_WARN(("No extended attributes buffer found. Not processing\n"));
		DEBUG_RET();
		return 0;
	}

	memcpy(&xattrib, &(buffer[bptr]), sizeof(xattrib));
	LE16_CPU(xattrib.extended);
	LE16_CPU(xattrib.zero);
	LE16_CPU(xattrib.type);
	LE16_CPU(xattrib.map);
	bptr += sizeof(xattrib);

	while (xattrib.type != 0 && bptr < bsize) {
		ptr = (pst_x_attrib_ll*) xmalloc(sizeof(pst_x_attrib_ll));
		memset(ptr, 0, sizeof(pst_x_attrib_ll));
		ptr->type = xattrib.type;
		ptr->map = xattrib.map+0x8000;
		ptr->next = NULL;
		DEBUG_INDEX(("xattrib: ext = %#hx, zero = %#hx, type = %#hx, map = %#hx\n",
			 xattrib.extended, xattrib.zero, xattrib.type, xattrib.map));
		err=0;
		if (xattrib.type & 0x0001) { // if the Bit 1 is set
			// pointer to Unicode field in buffer
			if (xattrib.extended < hsize) {
				char *wt;
				// copy the size of the header. It is 32 bit int
				memcpy(&tint, &(headerbuffer[xattrib.extended]), sizeof(tint));
				LE32_CPU(tint);
				wt = (char*) xmalloc(tint+2); // plus 2 for a uni-code zero
				memset(wt, 0, tint+2);
				memcpy(wt, &(headerbuffer[xattrib.extended+sizeof(tint)]), tint);
				ptr->data = _pst_wide_to_single(wt, tint);
				free(wt);
				DEBUG_INDEX(("Read string (converted from UTF-16): %s\n", ptr->data));
			} else {
				DEBUG_INDEX(("Cannot read outside of buffer [%i !< %i]\n", xattrib.extended, hsize));
			}
			ptr->mytype = PST_MAP_HEADER;
		} else {
			// contains the attribute code to map to.
			ptr->data = (int*)xmalloc(sizeof(int32_t));
			memset(ptr->data, 0, sizeof(int32_t));
			*((int32_t*)ptr->data) = xattrib.extended;
			ptr->mytype = PST_MAP_ATTRIB;
			DEBUG_INDEX(("Mapped attribute %#x to %#x\n", ptr->map, *((int32_t*)ptr->data)));
		}

		if (err==0) {
			// add it to the list
			p_sh = p_head;
			p_sh2 = NULL;
			while (p_sh && ptr->map > p_sh->map) {
				p_sh2 = p_sh;
				p_sh = p_sh->next;
			}
			if (!p_sh2) {
				// needs to go before first item
				ptr->next = p_head;
				p_head = ptr;
			} else {
				// it will go after p_sh2
				ptr->next = p_sh2->next;
				p_sh2->next = ptr;
			}
		} else {
			free(ptr);
			ptr = NULL;
		}
		memcpy(&xattrib, &(buffer[bptr]), sizeof(xattrib));
		LE16_CPU(xattrib.extended);
		LE16_CPU(xattrib.zero);
		LE16_CPU(xattrib.type);
		LE16_CPU(xattrib.map);
		bptr += sizeof(xattrib);
	}
	if (list2) _pst_free_id2(list2);
	if (na)    _pst_free_list(na);
	pf->x_head = p_head;
	DEBUG_RET();
	return 1;
}


#define BLOCK_SIZE			   516		// index blocks
#define DESC_BLOCK_SIZE 	   516		// descriptor blocks was 520 but bogus
#define ITEM_COUNT_OFFSET	   0x1f0	// count byte
#define LEVEL_INDICATOR_OFFSET 0x1f3	// node or leaf
#define BACKLINK_OFFSET 	   0x1f8	// backlink u1 value
#define ITEM_SIZE			   12
#define DESC_SIZE			   16
#define INDEX_COUNT_MAX 	   41		// max active items
#define DESC_COUNT_MAX		   31		// max active items

int32_t _pst_build_id_ptr(pst_file *pf, int32_t offset, int32_t depth, int32_t linku1, u_int32_t start_val, u_int32_t end_val) {
	struct _pst_table_ptr_struct table, table2;
	pst_index_ll *i_ptr=NULL;
	pst_index index;
	int32_t x, item_count;
	u_int32_t old = start_val;
	char *buf = NULL, *bptr;

	DEBUG_ENT("_pst_build_id_ptr");
	DEBUG_INDEX(("offset %x depth %i linku1 %x start %x end %x\n", offset, depth, linku1, start_val, end_val));
	if (end_val <= start_val) {
		DEBUG_WARN(("The end value is BEFORE the start value. This function will quit. Soz. [start:%#x, end:%#x]\n", start_val, end_val));
		DEBUG_RET();
		return -1;
	}
	DEBUG_INDEX(("Reading index block\n"));
	if (_pst_read_block_size(pf, offset, BLOCK_SIZE, &buf, 0, 0) < BLOCK_SIZE) {
		DEBUG_WARN(("Failed to read %i bytes\n", BLOCK_SIZE));
		if (buf) free(buf);
		DEBUG_RET();
		return -1;
	}
	bptr = buf;
	DEBUG_HEXDUMPC(buf, BLOCK_SIZE, ITEM_SIZE);
	item_count = (int)(unsigned)(buf[ITEM_COUNT_OFFSET]);
	if (item_count > INDEX_COUNT_MAX) {
		DEBUG_WARN(("Item count %i too large, max is %i\n", item_count, INDEX_COUNT_MAX));
		if (buf) free(buf);
		DEBUG_RET();
		return -1;
	}
	memcpy(&index, buf+BACKLINK_OFFSET, sizeof(index));
	LE32_CPU(index.id);
	if (index.id != linku1) {
		DEBUG_WARN(("Backlink %#x in this node does not match required %#x\n", index.id, linku1));
		if (buf) free(buf);
		DEBUG_RET();
		return -1;
	}

	if (buf[LEVEL_INDICATOR_OFFSET] == '\0') {
		// this node contains leaf pointers
		x = 0;
		while (x < item_count) {
			memcpy(&index, bptr, sizeof(index));
			LE32_CPU(index.id);
			LE32_CPU(index.offset);
			LE16_CPU(index.size);
			LE16_CPU(index.u1);
			bptr += sizeof(index);
			x++;
			if (index.id == 0) break;
			DEBUG_INDEX(("[%i]%i Item [id = %#x, offset = %#x, u1 = %#x, size = %i(%#x)]\n",
						depth, x, index.id, index.offset, index.u1, index.size, index.size));
			// if (index.id & 0x02) DEBUG_INDEX(("two-bit set!!\n"));
			if ((index.id >= end_val) || (index.id < old)) {
				DEBUG_WARN(("This item isn't right. Must be corruption, or I got it wrong!\n"));
				if (buf) free(buf);
				DEBUG_RET();
				return -1;
			}
			old = index.id;
			if (x == 1) {	// first entry
				if ((start_val) && (index.id != start_val)) {
					DEBUG_WARN(("This item isn't right. Must be corruption, or I got it wrong!\n"));
					if (buf) free(buf);
					DEBUG_RET();
					return -1;
				}
			}
			i_ptr = (pst_index_ll*) xmalloc(sizeof(pst_index_ll));
			i_ptr->id	  = index.id;
			i_ptr->offset = index.offset;
			i_ptr->u1	  = index.u1;
			i_ptr->size   = index.size;
			i_ptr->next   = NULL;
			if (pf->i_tail)  pf->i_tail->next = i_ptr;
			if (!pf->i_head) pf->i_head = i_ptr;
			pf->i_tail = i_ptr;
		}
	} else {
		// this node contains node pointers
		x = 0;
		while (x < item_count) {
			memcpy(&table, bptr, sizeof(table));
			LE32_CPU(table.start);
			LE32_CPU(table.u1);
			LE32_CPU(table.offset);
			bptr += sizeof(table);
			x++;
			if (table.start == 0) break;
			if (x < item_count) {
				memcpy(&table2, bptr, sizeof(table));
				LE32_CPU(table2.start);
			}
			else {
				table2.start = end_val;
			}
			DEBUG_INDEX(("[%i] %i Index Table [start id = %#x, u1 = %#x, offset = %#x, end id = %#x]\n",
						depth, x, table.start, table.u1, table.offset, table2.start));
			if ((table.start >= end_val) || (table.start < old)) {
				DEBUG_WARN(("This table isn't right. Must be corruption, or I got it wrong!\n"));
				if (buf) free(buf);
				DEBUG_RET();
				return -1;
			}
			old = table.start;
			if (x == 1) {	// first entry
				if ((start_val) && (table.start != start_val)) {
					DEBUG_WARN(("This table isn't right. Must be corruption, or I got it wrong!\n"));
					if (buf) free(buf);
					DEBUG_RET();
					return -1;
				}
			}
			_pst_build_id_ptr(pf, table.offset, depth+1, table.u1, table.start, table2.start);
		}
	}
	if (buf) free (buf);
	DEBUG_RET();
	return 0;
}


/** this list node type is used for a quick cache
	of the descriptor tree nodes (rooted at pf->d_head)
	and for a "lost and found" list.
	If the parent isn't found yet, put it on the lost and found
	list and check it each time you read a new item.
*/
struct cache_list_node {
	pst_desc_ll *ptr;
	/** only used for lost and found lists */
	u_int32_t parent;
	struct cache_list_node *next;
	struct cache_list_node *prev;
};
struct cache_list_node *cache_head;
struct cache_list_node *cache_tail;
struct cache_list_node *lostfound_head;
int32_t cache_count;


/**
	add the d_ptr descriptor into the global tree
*/
void record_descriptor(pst_file *pf, pst_desc_ll *d_ptr, u_int32_t parent_id) {
	struct cache_list_node *lostfound_ptr = NULL;
	struct cache_list_node *cache_ptr	  = NULL;
	pst_desc_ll 		   *parent		  = NULL;

	if (parent_id == 0 || parent_id == d_ptr->id) {
		// add top level node to the descriptor tree
		if (parent_id == 0) {
			DEBUG_INDEX(("No Parent\n"));
		} else {
			DEBUG_INDEX(("Record is its own parent. What is this world coming to?\n"));
		}
		if (pf->d_tail)  pf->d_tail->next = d_ptr;
		if (!pf->d_head) pf->d_head = d_ptr;
		d_ptr->prev = pf->d_tail;
		pf->d_tail	= d_ptr;
	} else {
		DEBUG_INDEX(("Searching for parent\n"));
		// check in the cache for the parent
		cache_ptr = cache_head;
		while (cache_ptr && (cache_ptr->ptr->id != parent_id)) {
			cache_ptr = cache_ptr->next;
		}
		if (!cache_ptr && (parent = _pst_getDptr(pf, parent_id)) == NULL) {
			// check in the lost/found list
			lostfound_ptr = lostfound_head;
			while (lostfound_ptr && (lostfound_ptr->ptr->id != parent_id)) {
				lostfound_ptr = lostfound_ptr->next;
			}
			if (!lostfound_ptr) {
				DEBUG_WARN(("ERROR -- cannot find parent with id %#x. Adding to lost/found\n", parent_id));
				lostfound_ptr = (struct cache_list_node*) xmalloc(sizeof(struct cache_list_node));
				lostfound_ptr->prev   = NULL;
				lostfound_ptr->next   = lostfound_head;
				lostfound_ptr->parent = parent_id;
				lostfound_ptr->ptr	  = d_ptr;
				lostfound_head = lostfound_ptr;
			} else {
				parent = lostfound_ptr->ptr;
				DEBUG_INDEX(("Found parent (%#x) in Lost and Found\n", parent->id));
			}
		}

		if (cache_ptr || parent) {
			if (cache_ptr)
				// parent is already in the cache
				parent = cache_ptr->ptr;
			else {
				//add the parent to the cache
				DEBUG_INDEX(("Cache addition\n"));
				cache_ptr = (struct cache_list_node*) xmalloc(sizeof(struct cache_list_node));
				cache_ptr->prev = NULL;
				cache_ptr->next = cache_head;
				cache_ptr->ptr	= parent;
				cache_head = cache_ptr;
				if (!cache_tail) cache_tail = cache_ptr;
				cache_count++;
				if (cache_count > 100) {
					DEBUG_INDEX(("trimming quick cache\n"));
					//remove one from the end
					cache_ptr  = cache_tail;
					cache_tail = cache_ptr->prev;
					free (cache_ptr);
					cache_count--;
				}
			}
			DEBUG_INDEX(("Found a parent\n"));
			parent->no_child++;
			d_ptr->parent = parent;
			if (parent->child_tail)  parent->child_tail->next = d_ptr;
			if (!parent->child) 	 parent->child = d_ptr;
			d_ptr->prev = parent->child_tail;
			parent->child_tail = d_ptr;
		}
	}
}

int32_t _pst_build_desc_ptr (pst_file *pf, int32_t offset, int32_t depth, int32_t linku1, u_int32_t *high_id, u_int32_t start_val, u_int32_t end_val) {
	struct _pst_table_ptr_struct table, table2;
	pst_desc desc_rec;
	pst_desc_ll *d_ptr=NULL, *parent=NULL;
	int32_t x, item_count;
	u_int32_t old = start_val;
	char *buf = NULL, *bptr;
	struct cache_list_node *cache_ptr = NULL;
	struct cache_list_node *lostfound_ptr = NULL;
	struct cache_list_node *lostfound_shd = NULL;
	struct cache_list_node *lostfound_tmp = NULL;

	if (depth == 0) {
		// initialize the linked list and lost/found list.
		cache_head = NULL;
		cache_tail = NULL;
		lostfound_head = NULL;
		cache_count = 0;
	}

	DEBUG_ENT("_pst_build_desc_ptr");
	DEBUG_INDEX(("offset %x depth %i linku1 %x start %x end %x\n", offset, depth, linku1, start_val, end_val));
	if (end_val <= start_val) {
		DEBUG_WARN(("The end value is BEFORE the start value. This function will quit. Soz. [start:%#x, end:%#x]\n", start_val, end_val));
		DEBUG_RET();
		return -1;
	}
	DEBUG_INDEX(("Reading desc block\n"));
	if (_pst_read_block_size(pf, offset, DESC_BLOCK_SIZE, &buf, 0, 0) < DESC_BLOCK_SIZE) {
		DEBUG_WARN(("Failed to read %i bytes\n", DESC_BLOCK_SIZE));
		if (buf) free(buf);
		DEBUG_RET();
		return -1;
	}
	bptr = buf;
	item_count = (int)(unsigned)(buf[ITEM_COUNT_OFFSET]);
	memcpy(&desc_rec.d_id, buf+BACKLINK_OFFSET, sizeof(u_int32_t)); // for valgrind, only have 3 ints here, not 4
	LE32_CPU(desc_rec.d_id);
	if (desc_rec.d_id != linku1) {
		DEBUG_WARN(("Backlink %#x in this node does not match required %#x\n", desc_rec.d_id, linku1));
		if (buf) free(buf);
		DEBUG_RET();
		return -1;
	}
	if (buf[LEVEL_INDICATOR_OFFSET] == '\0') {
		// this node contains leaf pointers
		DEBUG_HEXDUMPC(buf, DESC_BLOCK_SIZE, 16);
		if (item_count > DESC_COUNT_MAX) {
			DEBUG_WARN(("Item count %i too large, max is %i\n", item_count, DESC_COUNT_MAX));
			if (buf) free(buf);
			DEBUG_RET();
			return -1;
		}
		x = 0;
		while (x < item_count) {
			memcpy(&desc_rec, bptr, sizeof(desc_rec));
			LE32_CPU(desc_rec.d_id);
			LE32_CPU(desc_rec.desc_id);
			LE32_CPU(desc_rec.list_id);
			LE32_CPU(desc_rec.parent_id);
			bptr+= sizeof(desc_rec);
			x++;
			if (desc_rec.d_id == 0) break;
			DEBUG_INDEX(("[%i] Item(%#x) = [d_id = %#x, desc_id = %#x, list_id = %#x, parent_id = %#x]\n",
						depth, x, desc_rec.d_id, desc_rec.desc_id, desc_rec.list_id, desc_rec.parent_id));
			if ((desc_rec.d_id >= end_val) || (desc_rec.d_id < old)) {
				DEBUG_WARN(("This item isn't right. Must be corruption, or I got it wrong!\n"));
				DEBUG_HEXDUMPC(buf, DESC_BLOCK_SIZE, 16);
				if (buf) free(buf);
				DEBUG_RET();
				return -1;
			}
			old = desc_rec.d_id;
			if (x == 1) {	// first entry
				if (start_val && (desc_rec.d_id != start_val)) {
					DEBUG_WARN(("This item isn't right. Must be corruption, or I got it wrong!\n"));
					if (buf) free(buf);
					DEBUG_RET();
					return -1;
				}
			}
			// When duplicates found, just update the info.... perhaps this is correct functionality
			DEBUG_INDEX(("Searching for existing record\n"));
			if (desc_rec.d_id <= *high_id && (d_ptr = _pst_getDptr(pf, desc_rec.d_id))) {
				DEBUG_INDEX(("Updating Existing Values\n"));
				d_ptr->list_index = _pst_getID(pf, desc_rec.list_id);
				d_ptr->desc = _pst_getID(pf, desc_rec.desc_id);
				DEBUG_INDEX(("\tdesc = %#x\tlist_index=%#x\n",
						(d_ptr->desc==NULL?0:d_ptr->desc->id),
						(d_ptr->list_index==NULL?0:d_ptr->list_index->id)));
				if (d_ptr->parent && desc_rec.parent_id != d_ptr->parent->id) {
					DEBUG_INDEX(("WARNING -- Parent of record has changed. Moving it\n"));
					//hmmm, we must move the record.
					// first we must remove from current location
					//	 change previous record to point next to our next
					//	   if no previous, then use parent's child
					//	   if no parent then change pf->d_head;
					//	 change next's prev to our prev
					//	   if no next then change parent's child_tail
					//	   if no parent then change pf->d_tail
					if (d_ptr->prev)
						d_ptr->prev->next = d_ptr->next;
					else if (d_ptr->parent)
						d_ptr->parent->child = d_ptr->next;
					else
						pf->d_head = d_ptr->next;

					if (d_ptr->next)
						d_ptr->next->prev = d_ptr->prev;
					else if (d_ptr->parent)
						d_ptr->parent->child_tail = d_ptr->prev;
					else
						pf->d_tail = d_ptr->prev;

					d_ptr->prev   = NULL;
					d_ptr->next   = NULL;
					d_ptr->parent = NULL;
					record_descriptor(pf, d_ptr, desc_rec.parent_id);	// add to the global tree
				}
			} else {
				if (*high_id < desc_rec.d_id) {
					DEBUG_INDEX(("Updating New High\n"));
					*high_id = desc_rec.d_id;
				}
				DEBUG_INDEX(("New Record\n"));
				d_ptr = (pst_desc_ll*) xmalloc(sizeof(pst_desc_ll));
				d_ptr->id = desc_rec.d_id;
				d_ptr->list_index = _pst_getID(pf, desc_rec.list_id);
				d_ptr->desc = _pst_getID(pf, desc_rec.desc_id);
				d_ptr->prev = NULL;
				d_ptr->next = NULL;
				d_ptr->parent = NULL;
				d_ptr->child = NULL;
				d_ptr->child_tail = NULL;
				d_ptr->no_child = 0;
				record_descriptor(pf, d_ptr, desc_rec.parent_id);	// add to the global tree

			}
			// check here to see if d_ptr is the parent of any of the items in the lost / found list
			lostfound_ptr = lostfound_head;
			lostfound_shd = NULL;
			while (lostfound_ptr) {
				if (lostfound_ptr->parent == d_ptr->id) {
					DEBUG_INDEX(("Found a child  (%#x) of the current record. Joining to main structure.\n", lostfound_ptr->ptr->id));
					parent = d_ptr;
					d_ptr = lostfound_ptr->ptr;
					parent->no_child++;
					d_ptr->parent = parent;
					if (parent->child_tail) parent->child_tail->next = d_ptr;
					if (!parent->child) 	parent->child = d_ptr;
					d_ptr->prev = parent->child_tail;
					parent->child_tail = d_ptr;
					if (!lostfound_shd) lostfound_head		= lostfound_ptr->next;
					else				lostfound_shd->next = lostfound_ptr->next;
					lostfound_tmp = lostfound_ptr->next;
					free(lostfound_ptr);
					lostfound_ptr = lostfound_tmp;
				} else {
					lostfound_shd = lostfound_ptr;
					lostfound_ptr = lostfound_ptr->next;
				}
			}
		}
	} else {
		// this node contains node pointers
		DEBUG_HEXDUMPC(buf, DESC_BLOCK_SIZE, ITEM_SIZE);
		if (item_count > INDEX_COUNT_MAX) {
			DEBUG_WARN(("Item count %i too large, max is %i\n", item_count, INDEX_COUNT_MAX));
			if (buf) free(buf);
			DEBUG_RET();
			return -1;
		}
		x = 0;
		while (x < item_count) {
			memcpy(&table, bptr, sizeof(table));
			LE32_CPU(table.start);
			LE32_CPU(table.u1);
			LE32_CPU(table.offset);
			bptr += sizeof(table);
			x++;
			if (table.start == 0) break;
			if (x < item_count) {
				memcpy(&table2, bptr, sizeof(table));
				LE32_CPU(table2.start);
			}
			else {
				table2.start = end_val;
			}
			DEBUG_INDEX(("[%i] %i Descriptor Table [start id = %#x, u1 = %#x, offset = %#x, end id = %#x]\n",
						depth, x, table.start, table.u1, table.offset, table2.start));
			if ((table.start >= end_val) || (table.start < old)) {
				DEBUG_WARN(("This table isn't right. Must be corruption, or I got it wrong!\n"));
				if (buf) free(buf);
				DEBUG_RET();
				return -1;
			}
			old = table.start;
			if (x == 1) {	// first entry
				if (start_val != -1 && table.start != start_val) {
					DEBUG_WARN(("This table isn't right. Must be corruption, or I got it wrong!\n"));
					if (buf) free(buf);
					DEBUG_RET();
					return -1;
				}
			}
			_pst_build_desc_ptr(pf, table.offset, depth+1, table.u1, high_id, table.start, table2.start);
		}
	}
	if (depth == 0) {
		// free the quick cache
		while (cache_head) {
			cache_ptr = cache_head->next;
			free(cache_head);
			cache_head = cache_ptr;
		}
		// free the lost and found
		while (lostfound_head) {
			lostfound_ptr = lostfound_head->next;
			WARN(("unused lost/found item with parent %d))", lostfound_head->parent));
			free(lostfound_head);
			lostfound_head = lostfound_ptr;
		}
	}
	if (buf) free(buf);
	DEBUG_RET();
	return 0;
}


void* _pst_parse_item(pst_file *pf, pst_desc_ll *d_ptr) {
	pst_num_array * list;
	pst_index2_ll *id2_head = NULL;
	pst_index_ll *id_ptr = NULL;
	pst_item *item = NULL;
	pst_item_attach *attach = NULL;
	int x;
	DEBUG_ENT("_pst_parse_item");
	if (!d_ptr) {
		DEBUG_WARN(("you cannot pass me a NULL! I don't want it!\n"));
		DEBUG_RET();
		return NULL;
	}

	if (d_ptr->list_index) {
		id2_head = _pst_build_id2(pf, d_ptr->list_index, NULL);
		_pst_printID2ptr(id2_head);
	} else {
		DEBUG_WARN(("Have not been able to fetch any id2 values for this item. Brace yourself!\n"));
	}

	if (!d_ptr->desc) {
		DEBUG_WARN(("why is d_ptr->desc == NULL? I don't want to do anything else with this record\n"));
		if (id2_head) _pst_free_id2(id2_head);
		DEBUG_RET();
		return NULL;
	}

	if ((list = _pst_parse_block(pf, d_ptr->desc->id, id2_head)) == NULL) {
		DEBUG_WARN(("_pst_parse_block() returned an error for d_ptr->desc->id [%#x]\n", d_ptr->desc->id));
		if (id2_head) _pst_free_id2(id2_head);
		DEBUG_RET();
		return NULL;
	}

	item = (pst_item*) xmalloc(sizeof(pst_item));
	memset(item, 0, sizeof(pst_item));

	if (_pst_process(list, item, NULL)) {
		DEBUG_WARN(("_pst_process() returned non-zero value. That is an error\n"));
		if (item)	  _pst_freeItem(item);
		if (list)	  _pst_free_list(list);
		if (id2_head) _pst_free_id2(id2_head);
		DEBUG_RET();
		return NULL;
	}
	if (list) _pst_free_list(list);
	list = NULL; //_pst_process will free the items in the list

	if ((id_ptr = _pst_getID2(id2_head, 0x671))) {
		// attachements exist - so we will process them
		while (item->attach) {
			attach = item->attach->next;
			free(item->attach);
			item->attach = attach;
		}

		DEBUG_EMAIL(("ATTACHEMENT processing attachement\n"));
		if ((list = _pst_parse_block(pf, id_ptr->id, id2_head)) == NULL) {
			DEBUG_WARN(("ERROR error processing main attachment record\n"));
			if (item) _pst_freeItem(item);
			if (id2_head) _pst_free_id2(id2_head);
			DEBUG_RET();
			return NULL;
		}
		else {
			x = 0;
			while (x < list->count_array) {
				attach = (pst_item_attach*) xmalloc (sizeof(pst_item_attach));
				memset (attach, 0, sizeof(pst_item_attach));
				attach->next = item->attach;
				item->attach = attach;
				x++;
			}

			if (_pst_process(list, item, item->attach)) {
				DEBUG_WARN(("ERROR _pst_process() failed with attachments\n"));
				if (item)	  _pst_freeItem(item);
				if (list)	  _pst_free_list(list);
				if (id2_head) _pst_free_id2(id2_head);
				DEBUG_RET();
				return NULL;
			}
			if (list) _pst_free_list(list);
			list = NULL;

			// now we will have initial information of each attachment stored in item->attach...
			// we must now read the secondary record for each based on the id2 val associated with
			// each attachment
			attach = item->attach;
			while (attach) {
			  if ((id_ptr = _pst_getID2(id2_head, attach->id2_val))) {
				  // id_ptr is a record describing the attachment
				  // we pass NULL instead of id2_head cause we don't want it to
				  // load all the extra stuff here.
				  if ((list = _pst_parse_block(pf, id_ptr->id, NULL)) == NULL) {
					  DEBUG_WARN(("ERROR error processing an attachment record\n"));
					  attach = attach->next;
					  continue;
				  }
				  if (_pst_process(list, item, attach)) {
					  DEBUG_WARN(("ERROR _pst_process() failed with an attachment\n"));
					  if (list) _pst_free_list(list);
					  list = NULL;
					  attach = attach->next;
					  continue;
				  }
				  if (list) _pst_free_list(list);
				  list = NULL;
				  id_ptr = _pst_getID2(id2_head, attach->id2_val);
				  if (id_ptr) {
					  // id2_val has been updated to the ID2 value of the datablock containing the
					  // attachment data
					  attach->id_val = id_ptr->id;
				  } else {
					  DEBUG_WARN(("have not located the correct value for the attachment [%#x]\n", attach->id2_val));
				  }
			  } else {
				  DEBUG_WARN(("ERROR cannot locate id2 value %#x\n", attach->id2_val));
			  }
			  attach = attach->next;
			}
		}
	}

	_pst_free_id2(id2_head);
	id2_head = NULL;
	DEBUG_RET();
	return item;
}


void freeall(unsigned char *buf, pst_block_offset_pointer *p1,
								 pst_block_offset_pointer *p2,
								 pst_block_offset_pointer *p3,
								 pst_block_offset_pointer *p4,
								 pst_block_offset_pointer *p5,
								 pst_block_offset_pointer *p6,
								 pst_block_offset_pointer *p7) {
	if (buf) free(buf);
	if (p1->needfree) free(p1->from);
	if (p2->needfree) free(p2->from);
	if (p3->needfree) free(p3->from);
	if (p4->needfree) free(p4->from);
	if (p5->needfree) free(p5->from);
	if (p6->needfree) free(p6->from);
	if (p7->needfree) free(p7->from);
}


pst_num_array * _pst_parse_block(pst_file *pf, u_int32_t block_id, pst_index2_ll *i2_head) {
	unsigned char *buf = NULL;
	pst_num_array *na_ptr = NULL, *na_head = NULL;
	pst_block_offset_pointer block_offset1;
	pst_block_offset_pointer block_offset2;
	pst_block_offset_pointer block_offset3;
	pst_block_offset_pointer block_offset4;
	pst_block_offset_pointer block_offset5;
	pst_block_offset_pointer block_offset6;
	pst_block_offset_pointer block_offset7;
	u_int32_t size;
	u_int32_t x;
	u_int32_t num_recs;
	u_int32_t count_rec;
	u_int32_t num_list;
	u_int32_t cur_list;
	u_int32_t block_type;
	u_int32_t rec_size = 0;
	u_int32_t ind_ptr;
	unsigned char* list_start;
	unsigned char* t_ptr;
	unsigned char* fr_ptr;
	unsigned char* to_ptr;
	unsigned char* ind2_end;
	unsigned char* ind2_ptr = NULL;
	size_t read_size=0;
	pst_x_attrib_ll *mapptr;

	struct {
		u_int16_t type;
		u_int16_t ref_type;
		u_int32_t value;
	} table_rec;	//for type 1 (0xBCEC) blocks
	struct {
		u_int16_t ref_type;
		u_int16_t type;
		u_int16_t ind2_off;
		u_int8_t  size;
		u_int8_t  slot;
	} table2_rec;	//for type 2 (0x7CEC) blocks
	struct {
		u_int32_t id;
	} table3_rec;	//for type 3 (0x0101) blocks
	struct {
		u_int16_t index_offset;
		u_int16_t type;
		u_int32_t offset;
	} block_hdr;
	struct {
		unsigned char seven_c;
		unsigned char item_count;
		u_int16_t u1;
		u_int16_t u2;
		u_int16_t u3;
		u_int16_t rec_size;
		u_int32_t b_five_offset;
		u_int32_t ind2_offset;
		u_int16_t u7;
		u_int16_t u8;
	} seven_c_blk;
	struct _type_d_rec {
		u_int32_t id;
		u_int32_t u1;
	} * type_d_rec;

	DEBUG_ENT("_pst_parse_block");
	if ((read_size = _pst_ff_getIDblock_dec(pf, block_id, &buf)) == 0) {
		WARN(("Error reading block id %#x\n", block_id));
		if (buf) free (buf);
		DEBUG_RET();
		return NULL;
	}

	block_offset1.needfree = 0;
	block_offset2.needfree = 0;
	block_offset3.needfree = 0;
	block_offset4.needfree = 0;
	block_offset5.needfree = 0;
	block_offset6.needfree = 0;
	block_offset7.needfree = 0;

	memcpy(&block_hdr, buf, sizeof(block_hdr));
	LE16_CPU(block_hdr.index_offset);
	LE16_CPU(block_hdr.type);
	LE32_CPU(block_hdr.offset);
	DEBUG_EMAIL(("block header (index_offset=%#hx, type=%#hx, offset=%#hx\n", block_hdr.index_offset, block_hdr.type, block_hdr.offset));

	ind_ptr = block_hdr.index_offset;

	if (block_hdr.type == 0xBCEC) { //type 1
		block_type = 1;

		if (_pst_getBlockOffsetPointer(pf, i2_head, buf, read_size, ind_ptr, block_hdr.offset, &block_offset1)) {
			DEBUG_WARN(("internal error (bc.b5 offset %#x) in reading block id %#x\n", block_hdr.offset, block_id));
			freeall(buf, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
			DEBUG_RET();
			return NULL;
		}
		memcpy(&table_rec, block_offset1.from, sizeof(table_rec));
		LE16_CPU(table_rec.type);
		LE16_CPU(table_rec.ref_type);
		LE32_CPU(table_rec.value);
		DEBUG_EMAIL(("table_rec (type=%#hx, ref_type=%#hx, value=%#x)\n", table_rec.type, table_rec.ref_type, table_rec.value));

		if (table_rec.type != 0x02B5) {
			WARN(("Unknown second block constant - %#X for id %#x\n", table_rec.type, block_id));
			DEBUG_HEXDUMPC(buf, sizeof(table_rec), 0x10);
			freeall(buf, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
			DEBUG_RET();
			return NULL;
		}

		if (_pst_getBlockOffsetPointer(pf, i2_head, buf, read_size, ind_ptr, table_rec.value, &block_offset2)) {
			DEBUG_WARN(("internal error (bc.b5.desc offset) in reading block id %#x\n", table_rec.value, block_id));
			freeall(buf, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
			DEBUG_RET();
			return NULL;
		}
		list_start = block_offset2.from;
		to_ptr	   = block_offset2.to;
		num_list = (to_ptr - list_start)/sizeof(table_rec);
		num_recs = 1; // only going to be one object in these blocks
	}
	else if (block_hdr.type == 0x7CEC) { //type 2
		block_type = 2;

		if (_pst_getBlockOffsetPointer(pf, i2_head, buf, read_size, ind_ptr, block_hdr.offset, &block_offset3)) {
			DEBUG_WARN(("internal error (7c.7c offset %#x) in reading block id %#x\n", block_hdr.offset, block_id));
			freeall(buf, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
			DEBUG_RET();
			return NULL;
		}
		fr_ptr = block_offset3.from; //now got pointer to "7C block"
		memset(&seven_c_blk, 0, sizeof(seven_c_blk));
		memcpy(&seven_c_blk, fr_ptr, sizeof(seven_c_blk));
		LE16_CPU(seven_c_blk.u1);
		LE16_CPU(seven_c_blk.u2);
		LE16_CPU(seven_c_blk.u3);
		LE16_CPU(seven_c_blk.rec_size);
		LE32_CPU(seven_c_blk.b_five_offset);
		LE32_CPU(seven_c_blk.ind2_offset);
		LE16_CPU(seven_c_blk.u7);
		LE16_CPU(seven_c_blk.u8);

		list_start = fr_ptr + sizeof(seven_c_blk); // the list of item numbers start after this record

		if (seven_c_blk.seven_c != 0x7C) { // this would mean it isn't a 7C block!
			WARN(("Error. There isn't a 7C where I want to see 7C!\n"));
			freeall(buf, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
			DEBUG_RET();
			return NULL;
		}

		rec_size = seven_c_blk.rec_size;
		num_list = seven_c_blk.item_count;

		if (_pst_getBlockOffsetPointer(pf, i2_head, buf, read_size, ind_ptr, seven_c_blk.b_five_offset, &block_offset4)) {
			DEBUG_WARN(("internal error (7c.b5 offset %#x) in reading block id %#x\n", seven_c_blk.b_five_offset, block_id));
			freeall(buf, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
			DEBUG_RET();
			return NULL;
		}
		memcpy(&table_rec, block_offset4.from, sizeof(table_rec));
		LE16_CPU(table_rec.type);
		LE16_CPU(table_rec.ref_type);
		LE32_CPU(table_rec.value);

		if (table_rec.type != 0x04B5) { // different constant than a type 1 record
			WARN(("Unknown second block constant - %#X for id %#x\n", table_rec.type, block_id));
			freeall(buf, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
			DEBUG_RET();
			return NULL;
		}

		if (_pst_getBlockOffsetPointer(pf, i2_head, buf, read_size, ind_ptr, table_rec.value, &block_offset5)) {
			DEBUG_WARN(("internal error (7c.5b.desc offset %#x) in reading block id %#x\n", table_rec.value, block_id));
			freeall(buf, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
			DEBUG_RET();
			return NULL;
		}
		num_recs = (block_offset5.to - block_offset5.from) / 6; // this will give the number of records in this block

		if (_pst_getBlockOffsetPointer(pf, i2_head, buf, read_size, ind_ptr, seven_c_blk.ind2_offset, &block_offset6)) {
			DEBUG_WARN(("internal error (7c.ind2 offset %#x) in reading block id %#x\n", seven_c_blk.ind2_offset, block_id));
			freeall(buf, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
			DEBUG_RET();
			return NULL;
		}
		ind2_ptr = block_offset6.from;
		ind2_end = block_offset6.to;
	}
	else if (block_hdr.index_offset == 0x0101) { //type 2
		unsigned char *buf2 = NULL;
		int n = block_hdr.type; // count
		int m = sizeof(table3_rec);
		int i;
		block_type = 3;
		for (i=0; i<n; i++) {
			memcpy(&table3_rec, buf+8+i*m, m);
			LE32_CPU(table3_rec.id);
			_pst_ff_getIDblock_dec(pf, table3_rec.id, &buf2);
			if (buf2) free(buf2);
			buf2 = NULL;
		}
		freeall(buf, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
		DEBUG_RET();
		return NULL;
	} else {
		WARN(("ERROR: Unknown block constant - %#X for id %#x\n", block_hdr.type, block_id));
		DEBUG_HEXDUMPC(buf, read_size,0x10);
		freeall(buf, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
		DEBUG_RET();
		return NULL;
	}

	DEBUG_EMAIL(("Mallocing number of records %i\n", num_recs));
	for (count_rec=0; count_rec<num_recs; count_rec++) {
		na_ptr = (pst_num_array*) xmalloc(sizeof(pst_num_array));
		memset(na_ptr, 0, sizeof(pst_num_array));
		na_ptr->next = na_head;
		na_head = na_ptr;
		// allocate an array of count num_recs to contain sizeof(struct_pst_num_item)
		na_ptr->items		= (struct _pst_num_item**) xmalloc(sizeof(struct _pst_num_item)*num_list);
		na_ptr->count_item	= num_list;
		na_ptr->orig_count	= num_list;
		na_ptr->count_array = num_recs; // each record will have a record of the total number of records
		for (x=0; x<num_list; x++) na_ptr->items[x] = NULL;
		x = 0;

		DEBUG_EMAIL(("going to read %i (%#x) items\n", na_ptr->count_item, na_ptr->count_item));

		fr_ptr = list_start; // initialize fr_ptr to the start of the list.
		for (cur_list=0; cur_list<num_list; cur_list++) { //we will increase fr_ptr as we progress through index
			unsigned char* value_pointer = NULL;	// needed for block type 2 with values larger than 4 bytes
			int value_size = 0;
			if (block_type == 1) {
				memcpy(&table_rec, fr_ptr, sizeof(table_rec));
				LE16_CPU(table_rec.type);
				LE16_CPU(table_rec.ref_type);
				//LE32_CPU(table_rec.value);	// done later, some may be order invariant
				fr_ptr += sizeof(table_rec);
			} else if (block_type == 2) {
				// we will copy the table2_rec values into a table_rec record so that we can keep the rest of the code
				memcpy(&table2_rec, fr_ptr, sizeof(table2_rec));
				LE16_CPU(table2_rec.ref_type);
				LE16_CPU(table2_rec.type);
				LE16_CPU(table2_rec.ind2_off);

				// table_rec and table2_rec are arranged differently, so assign the values across
				table_rec.type	   = table2_rec.type;
				table_rec.ref_type = table2_rec.ref_type;
				table_rec.value    = 0;
				if ((ind2_end - ind2_ptr) >= (table2_rec.ind2_off + table2_rec.size)) {
					int n = table2_rec.size;
					int m = sizeof(table_rec.value);
					if (n <= m) {
						memcpy(&table_rec.value, ind2_ptr + table2_rec.ind2_off, n);
					}
					else {
						value_pointer = ind2_ptr + table2_rec.ind2_off;
						value_size	  = n;
					}
					//LE32_CPU(table_rec.value);	// done later, some may be order invariant
				}
				else {
					DEBUG_WARN (("Trying to read outside buffer, buffer size %#x, offset %#x, data size %#x\n",
								read_size, ind2_end-ind2_ptr+table2_rec.ind2_off, table2_rec.size));
				}
				fr_ptr += sizeof(table2_rec);
			} else {
				WARN(("Missing code for block_type %i\n", block_type));
				freeall(buf, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
				if (na_head) _pst_free_list(na_head);
				DEBUG_RET();
				return NULL;
			}
			DEBUG_EMAIL(("reading block %i (type=%#x, ref_type=%#x, value=%#x)\n",
				x, table_rec.type, table_rec.ref_type, table_rec.value));

			if (!na_ptr->items[x]) {
				na_ptr->items[x] = (struct _pst_num_item*) xmalloc(sizeof(struct _pst_num_item));
			}
			memset(na_ptr->items[x], 0, sizeof(struct _pst_num_item)); //init it

			// check here to see if the id of the attribute is a mapped one
			mapptr = pf->x_head;
			while (mapptr && (mapptr->map < table_rec.type)) mapptr = mapptr->next;
			if (mapptr && (mapptr->map == table_rec.type)) {
				if (mapptr->mytype == PST_MAP_ATTRIB) {
					na_ptr->items[x]->id = *((int*)mapptr->data);
					DEBUG_EMAIL(("Mapped attrib %#x to %#x\n", table_rec.type, na_ptr->items[x]->id));
				} else if (mapptr->mytype == PST_MAP_HEADER) {
					DEBUG_EMAIL(("Internet Header mapping found %#x\n", table_rec.type));
					na_ptr->items[x]->id = PST_ATTRIB_HEADER;
					na_ptr->items[x]->extra = mapptr->data;
				}
			} else {
				na_ptr->items[x]->id = table_rec.type;
			}
			na_ptr->items[x]->type = 0; // checked later before it is set
			/* Reference Types
				0x0002 - Signed 16bit value
				0x0003 - Signed 32bit value
				0x0004 - 4-byte floating point
				0x0005 - Floating point double
				0x0006 - Signed 64-bit int
				0x0007 - Application Time
				0x000A - 32-bit error value
				0x000B - Boolean (non-zero = true)
				0x000D - Embedded Object
				0x0014 - 8-byte signed integer (64-bit)
				0x001E - Null terminated String
				0x001F - Unicode string
				0x0040 - Systime - Filetime structure
				0x0048 - OLE Guid
				0x0102 - Binary data
				0x1003 - Array of 32bit values
				0x1014 - Array of 64bit values
				0x101E - Array of Strings
				0x1102 - Array of Binary data
			*/

			if (table_rec.ref_type == 0x0002 ||
				table_rec.ref_type == 0x0003 ||
				table_rec.ref_type == 0x000b) {
				//contains 32 bits of data
				na_ptr->items[x]->size = sizeof(int32_t);
				na_ptr->items[x]->type = table_rec.ref_type;
				na_ptr->items[x]->data = xmalloc(sizeof(int32_t));
				memcpy(na_ptr->items[x]->data, &(table_rec.value), sizeof(int32_t));

			} else if (table_rec.ref_type == 0x0005 ||
					   table_rec.ref_type == 0x000d ||
					   table_rec.ref_type == 0x0014 ||
					   table_rec.ref_type == 0x001e ||
					   table_rec.ref_type == 0x0040 ||
					   table_rec.ref_type == 0x0048 ||
					   table_rec.ref_type == 0x0102 ||
					   table_rec.ref_type == 0x1003 ||
					   table_rec.ref_type == 0x1014 ||
					   table_rec.ref_type == 0x101e ||
					   table_rec.ref_type == 0x1102) {
				//contains index reference to data
				LE32_CPU(table_rec.value);
				if (value_pointer) {
					// in a type 2 block, with a value that is more than 4 bytes
					// directly stored in this block.
					na_ptr->items[x]->size = value_size;
					na_ptr->items[x]->type = table_rec.ref_type;
					na_ptr->items[x]->data = xmalloc(value_size);
					memcpy(na_ptr->items[x]->data, value_pointer, value_size);
				}
				else if (_pst_getBlockOffsetPointer(pf, i2_head, buf, read_size, ind_ptr, table_rec.value, &block_offset7)) {
					if ((table_rec.value & 0xf) == 0xf) {
						DEBUG_WARN(("failed to get block offset for table_rec.value of %#x to be read later.\n", table_rec.value));
						na_ptr->items[x]->size = 0;
						na_ptr->items[x]->data = NULL;
						na_ptr->items[x]->type = table_rec.value;
					}
					else {
						if (table_rec.value) {
							DEBUG_WARN(("failed to get block offset for table_rec.value of %#x\n", table_rec.value));
						}
						na_ptr->count_item --; //we will be skipping a row
						continue;
					}
				}
				else {
					value_size = block_offset7.to - block_offset7.from;
					na_ptr->items[x]->size = value_size;
					na_ptr->items[x]->type = table_rec.ref_type;
					na_ptr->items[x]->data = xmalloc(value_size+1);
					memcpy(na_ptr->items[x]->data, block_offset7.from, value_size);
					na_ptr->items[x]->data[value_size] = '\0';  // it might be a string, null terminate it.
				}
				if (table_rec.ref_type == 0xd) {
					// there is still more to do for the type of 0xD
					type_d_rec = (struct _type_d_rec*) na_ptr->items[x]->data;
					LE32_CPU(type_d_rec->id);
					na_ptr->items[x]->size = _pst_ff_getID2block(pf, type_d_rec->id, i2_head, &(na_ptr->items[x]->data));
					if (!na_ptr->items[x]->size){
						DEBUG_WARN(("not able to read the ID2 data. Setting to be read later. %#x\n", type_d_rec->id));
						na_ptr->items[x]->type = type_d_rec->id;	// fetch before freeing data, alias pointer
						free(na_ptr->items[x]->data);
						na_ptr->items[x]->data = NULL;
					}
				}
				if (na_ptr->items[x]->type == 0) na_ptr->items[x]->type = table_rec.ref_type;
			} else {
				WARN(("ERROR Unknown ref_type %#x\n", table_rec.ref_type));
				freeall(buf, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
				if (na_head) _pst_free_list(na_head);
				DEBUG_RET();
				return NULL;
			}
			x++;
		}
		DEBUG_EMAIL(("increasing ind2_ptr by %i [%#x] bytes. Was %#x, Now %#x\n", rec_size, rec_size, ind2_ptr, ind2_ptr+rec_size));
		ind2_ptr += rec_size;
	}
	freeall(buf, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
	DEBUG_RET();
	return na_head;
}


// check if item->email is NULL, and init if so
#define MALLOC_EMAIL(x) 	   { if (!x->email) 		{ x->email		   = (pst_item_email*)		   xmalloc(sizeof(pst_item_email)); 		memset(x->email,		 0, sizeof(pst_item_email)		  );} }
#define MALLOC_FOLDER(x)	   { if (!x->folder)		{ x->folder 	   = (pst_item_folder*) 	   xmalloc(sizeof(pst_item_folder));		memset(x->folder,		 0, sizeof(pst_item_folder) 	  );} }
#define MALLOC_CONTACT(x)	   { if (!x->contact)		{ x->contact	   = (pst_item_contact*)	   xmalloc(sizeof(pst_item_contact));		memset(x->contact,		 0, sizeof(pst_item_contact)	  );} }
#define MALLOC_MESSAGESTORE(x) { if (!x->message_store) { x->message_store = (pst_item_message_store*) xmalloc(sizeof(pst_item_message_store)); memset(x->message_store, 0, sizeof(pst_item_message_store));} }
#define MALLOC_JOURNAL(x)	   { if (!x->journal)		{ x->journal	   = (pst_item_journal*)	   xmalloc(sizeof(pst_item_journal));		memset(x->journal,		 0, sizeof(pst_item_journal)	  );} }
#define MALLOC_APPOINTMENT(x)  { if (!x->appointment)	{ x->appointment   = (pst_item_appointment*)   xmalloc(sizeof(pst_item_appointment));	memset(x->appointment,	 0, sizeof(pst_item_appointment)  );} }
// malloc space and copy the current item's data -- plus one on the size for good luck (and string termination)
#define LIST_COPY(targ, type) { \
	targ = type realloc(targ, list->items[x]->size+1); \
	memset(targ, 0, list->items[x]->size+1); \
	memcpy(targ, list->items[x]->data, list->items[x]->size); \
}

#define NULL_CHECK(x) { if (!x) { DEBUG_EMAIL(("NULL_CHECK: Null Found\n")); break;} }

#define MOVE_NEXT(targ) { \
	if (next){\
		if (!targ) {\
			DEBUG_EMAIL(("MOVE_NEXT: Target is NULL. Will stop processing this option\n"));\
			break;\
		}\
		targ = targ->next;\
		if (!targ) {\
			DEBUG_EMAIL(("MOVE_NEXT: Target is NULL after next. Will stop processing this option\n"));\
			break;\
		}\
		next=0;\
	}\
}


int32_t _pst_process(pst_num_array *list , pst_item *item, pst_item_attach *attach) {
	int32_t x, t;
	int32_t next = 0;
	pst_item_extra_field *ef;

	DEBUG_ENT("_pst_process");
	if (!item) {
		DEBUG_EMAIL(("item cannot be NULL.\n"));
		DEBUG_RET();
		return -1;
	}

	while (list) {
		x = 0;
		while (x < list->count_item) {
			// check here to see if the id is one that is mapped.
			DEBUG_EMAIL(("#%d - id: %#x type: %#x length: %#x\n", x, list->items[x]->id, list->items[x]->type, list->items[x]->size));

			switch (list->items[x]->id) {
				case PST_ATTRIB_HEADER: // CUSTOM attribute for saying the Extra Headers
					DEBUG_EMAIL(("Extra Field - "));
					ef = (pst_item_extra_field*) xmalloc(sizeof(pst_item_extra_field));
					memset(ef, 0, sizeof(pst_item_extra_field));
					ef->field_name = (char*) xmalloc(strlen(list->items[x]->extra)+1);
					strcpy(ef->field_name, list->items[x]->extra);
					LIST_COPY(ef->value, (char*));
					ef->next = item->extra_fields;
					item->extra_fields = ef;
					DEBUG_EMAIL(("\"%s\" = \"%s\"\n", ef->field_name, ef->value));
					break;
				case 0x0002: // PR_ALTERNATE_RECIPIENT_ALLOWED
					// If set to true, the sender allows this email to be autoforwarded
					DEBUG_EMAIL(("AutoForward allowed - "));
					MALLOC_EMAIL(item);
					if (*((short int*)list->items[x]->data) != 0) {
						DEBUG_EMAIL(("True\n"));
						item->email->autoforward = 1;
					} else {
						DEBUG_EMAIL(("False\n"));
						item->email->autoforward = -1;
					}
					break;
				case 0x0003: // Extended Attributes table
					DEBUG_EMAIL(("Extended Attributes Table - NOT PROCESSED\n"));
					break;
				case 0x0017: // PR_IMPORTANCE
					// How important the sender deems it to be
					// 0 - Low
					// 1 - Normal
					// 2 - High

					DEBUG_EMAIL(("Importance Level - "));
					MALLOC_EMAIL(item);
					memcpy(&(item->email->importance), list->items[x]->data, sizeof(item->email->importance));
					LE32_CPU(item->email->importance);
					t = item->email->importance;
					DEBUG_EMAIL(("%s [%i]\n", (t==0?"Low":(t==1?"Normal":"High")), t));
					break;
				case 0x001A: // PR_MESSAGE_CLASS Ascii type of messages - NOT FOLDERS
					// must be case insensitive
					DEBUG_EMAIL(("IPM.x - "));
					LIST_COPY(item->ascii_type, (char*));
					if (pst_strincmp("IPM.Note", item->ascii_type, 8) == 0)
						// the string begins with IPM.Note...
						item->type = PST_TYPE_NOTE;
					else if (pst_stricmp("IPM", item->ascii_type) == 0)
						// the whole string is just IPM
						item->type = PST_TYPE_NOTE;
					else if (pst_strincmp("IPM.Contact", item->ascii_type, 11) == 0)
						// the string begins with IPM.Contact...
						item->type = PST_TYPE_CONTACT;
					else if (pst_strincmp("REPORT.IPM.Note", item->ascii_type, 15) == 0)
						// the string begins with the above
						item->type = PST_TYPE_REPORT;
					else if (pst_strincmp("IPM.Activity", item->ascii_type, 12) == 0)
						item->type = PST_TYPE_JOURNAL;
					else if (pst_strincmp("IPM.Appointment", item->ascii_type, 15) == 0)
						item->type = PST_TYPE_APPOINTMENT;
					else
						item->type = PST_TYPE_OTHER;

					DEBUG_EMAIL(("%s\n", item->ascii_type));
					break;
				case 0x0023: // PR_ORIGINATOR_DELIVERY_REPORT_REQUESTED
					// set if the sender wants a delivery report from all recipients
					DEBUG_EMAIL(("Global Delivery Report - "));
					MALLOC_EMAIL(item);
					if (*(int16_t*)list->items[x]->data != 0) {
						DEBUG_EMAIL(("True\n"));
						item->email->delivery_report = 1;
					} else {
						DEBUG_EMAIL(("False\n"));
						item->email->delivery_report = 0;
					}
					break;
				case 0x0026: // PR_PRIORITY
					// Priority of a message
					// -1 NonUrgent
					//	0 Normal
					//	1 Urgent
					DEBUG_EMAIL(("Priority - "));
					MALLOC_EMAIL(item);
					memcpy(&(item->email->priority), list->items[x]->data, sizeof(item->email->priority));
					LE32_CPU(item->email->priority);
					t = item->email->priority;
					DEBUG_EMAIL(("%s [%i]\n", (t<0?"NonUrgent":(t==0?"Normal":"Urgent")), t));
					break;
				case 0x0029:// PR_READ_RECEIPT_REQUESTED
					DEBUG_EMAIL(("Read Receipt - "));
					MALLOC_EMAIL(item);
					if (*(short int*)list->items[x]->data != 0) {
						DEBUG_EMAIL(("True\n"));
						item->email->read_receipt = 1;
					} else {
						DEBUG_EMAIL(("False\n"));
						item->email->read_receipt = 0;
					}
					break;
				case 0x002B: // PR_RECIPIENT_REASSIGNMENT_PROHIBITED
					DEBUG_EMAIL(("Reassignment Prohibited (Private) - "));
					if (*(short int*)list->items[x]->data != 0) {
						DEBUG_EMAIL(("True\n"));
						item->private_member = 1;
					} else {
						DEBUG_EMAIL(("False\n"));
						item->private_member = 0;
					}
					break;
				case 0x002E: // PR_ORIGINAL_SENSITIVITY
					// the sensitivity of the message before being replied to or forwarded
					// 0 - None
					// 1 - Personal
					// 2 - Private
					// 3 - Company Confidential
					DEBUG_EMAIL(("Original Sensitivity - "));
					MALLOC_EMAIL(item);
					memcpy(&(item->email->orig_sensitivity), list->items[x]->data, sizeof(item->email->orig_sensitivity));
					LE32_CPU(item->email->orig_sensitivity);
					t = item->email->orig_sensitivity;
					DEBUG_EMAIL(("%s [%i]\n", (t==0?"None":(t==1?"Personal":
										(t==2?"Private":"Company Confidential"))), t));
					break;
				case 0x0036: // PR_SENSITIVITY
					// sender's opinion of the sensitivity of an email
					// 0 - None
					// 1 - Personal
					// 2 - Private
					// 3 - Company Confidiential
					DEBUG_EMAIL(("Sensitivity - "));
					MALLOC_EMAIL(item);
					memcpy(&(item->email->sensitivity), list->items[x]->data, sizeof(item->email->sensitivity));
					LE32_CPU(item->email->sensitivity);
					t = item->email->sensitivity;
					DEBUG_EMAIL(("%s [%i]\n", (t==0?"None":(t==1?"Personal":
										(t==2?"Private":"Company Confidential"))), t));
					break;
				case 0x0037: // PR_SUBJECT raw subject
					//		if (list->items[x]->id == 0x0037) {
					DEBUG_EMAIL(("Raw Subject - "));
					MALLOC_EMAIL(item);
					item->email->subject = (pst_item_email_subject*) realloc(item->email->subject, sizeof(pst_item_email_subject));
					memset(item->email->subject, 0, sizeof(pst_item_email_subject));
					DEBUG_EMAIL((" [size = %i] ", list->items[x]->size));
					if (list->items[x]->size > 0) {
						if (isprint(list->items[x]->data[0])) {
							// then there are no control bytes at the front
							item->email->subject->off1 = 0;
							item->email->subject->off2 = 0;
							item->email->subject->subj = realloc(item->email->subject->subj, list->items[x]->size+1);
							memset(item->email->subject->subj, 0, list->items[x]->size+1);
							memcpy(item->email->subject->subj, list->items[x]->data, list->items[x]->size);
						} else {
							DEBUG_EMAIL(("Raw Subject has control codes\n"));
							// there might be some control bytes in the first and second bytes
							item->email->subject->off1 = list->items[x]->data[0];
							item->email->subject->off2 = list->items[x]->data[1];
							item->email->subject->subj = realloc(item->email->subject->subj, (list->items[x]->size-2)+1);
							memset(item->email->subject->subj, 0, list->items[x]->size-1);
							memcpy(item->email->subject->subj, &(list->items[x]->data[2]), list->items[x]->size-2);
						}
						DEBUG_EMAIL(("%s\n", item->email->subject->subj));
					} else {
						// obviously outlook has decided not to be straight with this one.
						item->email->subject->off1 = 0;
						item->email->subject->off2 = 0;
						item->email->subject = NULL;
						DEBUG_EMAIL(("NULL subject detected\n"));
					}
					break;
				case 0x0039: // PR_CLIENT_SUBMIT_TIME Date Email Sent/Created
					DEBUG_EMAIL(("Date sent - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->sent_date, (FILETIME*));
					LE32_CPU(item->email->sent_date->dwLowDateTime);
					LE32_CPU(item->email->sent_date->dwHighDateTime);
					DEBUG_EMAIL(("%s", fileTimeToAscii(item->email->sent_date)));
					break;
				case 0x003B: // PR_SENT_REPRESENTING_SEARCH_KEY Sender address 1
					DEBUG_EMAIL(("Sent on behalf of address 1 - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->outlook_sender, (char*));
					DEBUG_EMAIL(("%s\n", item->email->outlook_sender));
					break;
				case 0x003F: // PR_RECEIVED_BY_ENTRYID Structure containing Recipient
					DEBUG_EMAIL(("Recipient Structure 1 -- NOT HANDLED\n"));
					break;
				case 0x0040: // PR_RECEIVED_BY_NAME Name of Recipient Structure
					DEBUG_EMAIL(("Received By Name 1 -- NOT HANDLED\n"));
					break;
				case 0x0041: // PR_SENT_REPRESENTING_ENTRYID Structure containing Sender
					DEBUG_EMAIL(("Sent on behalf of Structure 1 -- NOT HANDLED\n"));
					break;
				case 0x0042: // PR_SENT_REPRESENTING_NAME Name of Sender Structure
					DEBUG_EMAIL(("Sent on behalf of Structure Name - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->outlook_sender_name, (char*));
					DEBUG_EMAIL(("%s\n", item->email->outlook_sender_name));
					break;
				case 0x0043: // PR_RCVD_REPRESENTING_ENTRYID Recipient Structure 2
					DEBUG_EMAIL(("Received on behalf of Structure -- NOT HANDLED\n"));
					break;
				case 0x0044: // PR_RCVD_REPRESENTING_NAME Name of Recipient Structure 2
					DEBUG_EMAIL(("Received on behalf of Structure Name -- NOT HANDLED\n"));
					break;
				case 0x004F: // PR_REPLY_RECIPIENT_ENTRIES Reply-To Structure
					DEBUG_EMAIL(("Reply-To Structure -- NOT HANDLED\n"));
					break;
				case 0x0050: // PR_REPLY_RECIPIENT_NAMES Name of Reply-To Structure
					DEBUG_EMAIL(("Name of Reply-To Structure -"));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->reply_to, (char*));
					DEBUG_EMAIL(("%s\n", item->email->reply_to));
					break;
				case 0x0051: // PR_RECEIVED_BY_SEARCH_KEY Recipient Address 1
					DEBUG_EMAIL(("Recipient's Address 1 (Search Key) - "));
					MALLOC_EMAIL(item);
					LIST_COPY (item->email->outlook_recipient, (char*));
					DEBUG_EMAIL(("%s\n", item->email->outlook_recipient));
					break;
				case 0x0052: // PR_RCVD_REPRESENTING_SEARCH_KEY Recipient Address 2
					DEBUG_EMAIL(("Received on behalf of Address (Search Key) - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->outlook_recipient2, (char*));
					DEBUG_EMAIL(("%s\n", item->email->outlook_recipient2));
					break;
				case 0x0057: // PR_MESSAGE_TO_ME
					// this user is listed explicitly in the TO address
					DEBUG_EMAIL(("My address in TO field - "));
					MALLOC_EMAIL(item);
					if (*(int16_t*)list->items[x]->data != 0) {
						DEBUG_EMAIL(("True\n"));
						item->email->message_to_me = 1;
					} else {
						DEBUG_EMAIL(("False\n"));
						item->email->message_to_me = 0;
					}
					break;
				case 0x0058: // PR_MESSAGE_CC_ME
					// this user is listed explicitly in the CC address
					DEBUG_EMAIL(("My address in CC field - "));
					MALLOC_EMAIL(item);
					if (*(int16_t*)list->items[x]->data != 0) {
						DEBUG_EMAIL(("True\n"));
						item->email->message_cc_me = 1;
					} else {
						DEBUG_EMAIL(("False\n"));
						item->email->message_cc_me = 0;
					}
					break;
				case 0x0059: //PR_MESSAGE_RECIP_ME
					// this user appears in TO, CC or BCC address list
					DEBUG_EMAIL(("Message addressed to me - "));
					MALLOC_EMAIL(item);
					if (*(int16_t*)list->items[x]->data != 0) {
						DEBUG_EMAIL(("True\n"));
						item->email->message_recip_me = 1;
					} else {
						DEBUG_EMAIL(("False\n"));
						item->email->message_recip_me = 0;
					}
					break;
				case 0x0063: // PR_RESPONSE_REQUESTED
					DEBUG_EMAIL(("Response requested - "));
					if (*(int16_t*)list->items[x]->data != 0) {
						DEBUG_EMAIL(("True\n"));
						item->response_requested = 1;
					} else {
						DEBUG_EMAIL(("False\n"));
						item->response_requested = 0;
					}
					break;
				case 0x0064: // PR_SENT_REPRESENTING_ADDRTYPE Access method for Sender Address
					DEBUG_EMAIL(("Sent on behalf of address type - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->sender_access, (char*));
					DEBUG_EMAIL(("%s\n", item->email->sender_access));
					break;
				case 0x0065: // PR_SENT_REPRESENTING_EMAIL_ADDRESS Sender Address
					DEBUG_EMAIL(("Sent on behalf of Address - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->sender_address, (char*));
					DEBUG_EMAIL(("%s\n", item->email->sender_address));
					break;
				case 0x0070: // PR_CONVERSATION_TOPIC Processed Subject
					DEBUG_EMAIL(("Processed Subject (Conversation Topic) - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->proc_subject, (char*));
					DEBUG_EMAIL(("%s\n", item->email->proc_subject));
					break;
				case 0x0071: // PR_CONVERSATION_INDEX Date 2
					DEBUG_EMAIL(("Conversation Index - "));
					MALLOC_EMAIL(item);
					memcpy(&(item->email->conv_index), list->items[x]->data, sizeof(item->email->conv_index));
					DEBUG_EMAIL(("%i\n", item->email->conv_index));
					break;
				case 0x0075: // PR_RECEIVED_BY_ADDRTYPE Recipient Access Method
					DEBUG_EMAIL(("Received by Address type - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->recip_access, (char*));
					DEBUG_EMAIL(("%s\n", item->email->recip_access));
					break;
				case 0x0076: // PR_RECEIVED_BY_EMAIL_ADDRESS Recipient Address
					DEBUG_EMAIL(("Received by Address - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->recip_address, (char*));
					DEBUG_EMAIL(("%s\n", item->email->recip_address));
					break;
				case 0x0077: // PR_RCVD_REPRESENTING_ADDRTYPE Recipient Access Method 2
					DEBUG_EMAIL(("Received on behalf of Address type - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->recip2_access, (char*));
					DEBUG_EMAIL(("%s\n", item->email->recip2_access));
					break;
				case 0x0078: // PR_RCVD_REPRESENTING_EMAIL_ADDRESS Recipient Address 2
					DEBUG_EMAIL(("Received on behalf of Address -"));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->recip2_address, (char*));
					DEBUG_EMAIL(("%s\n", item->email->recip2_address));
					break;
				case 0x007D: // PR_TRANSPORT_MESSAGE_HEADERS Internet Header
					DEBUG_EMAIL(("Internet Header - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->header, (char*));
					//DEBUG_EMAIL(("%s\n", item->email->header));
					DEBUG_EMAIL(("NOT PRINTED\n"));
					break;
				case 0x0C17: // PR_REPLY_REQUESTED
					DEBUG_EMAIL(("Reply Requested - "));
					MALLOC_EMAIL(item);
					if (*(int16_t*)list->items[x]->data != 0) {
						DEBUG_EMAIL(("True\n"));
						item->email->reply_requested = 1;
					} else {
						DEBUG_EMAIL(("False\n"));
						item->email->reply_requested = 0;
					}
					break;
				case 0x0C19: // PR_SENDER_ENTRYID Sender Structure 2
					DEBUG_EMAIL(("Sender Structure 2 -- NOT HANDLED\n"));
					break;
				case 0x0C1A: // PR_SENDER_NAME Name of Sender Structure 2
					DEBUG_EMAIL(("Name of Sender Structure 2 -- NOT HANDLED\n"));
					break;
				case 0x0C1D: // PR_SENDER_SEARCH_KEY Name of Sender Address 2
					DEBUG_EMAIL(("Name of Sender Address 2 (Sender search key) - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->outlook_sender2, (char*));
					DEBUG_EMAIL(("%s\n", item->email->outlook_sender2));
					break;
				case 0x0C1E: // PR_SENDER_ADDRTYPE Sender Address 2 access method
					DEBUG_EMAIL(("Sender Address type - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->sender2_access, (char*));
					DEBUG_EMAIL(("%s\n", item->email->sender2_access));
					break;
				case 0x0C1F: // PR_SENDER_EMAIL_ADDRESS Sender Address 2
					DEBUG_EMAIL(("Sender Address - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->sender2_address, (char*));
					DEBUG_EMAIL(("%s\n", item->email->sender2_address));
					break;
				case 0x0E01: // PR_DELETE_AFTER_SUBMIT
					// I am not too sure how this works
					DEBUG_EMAIL(("Delete after submit - "));
					MALLOC_EMAIL(item);
					if (*(int16_t*) list->items[x]->data != 0) {
						DEBUG_EMAIL(("True\n"));
						item->email->delete_after_submit = 1;
					} else {
						DEBUG_EMAIL(("False\n"));
						item->email->delete_after_submit = 0;
					}
					break;
				case 0x0E03: // PR_DISPLAY_CC CC Addresses
					DEBUG_EMAIL(("Display CC Addresses - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->cc_address, (char*));
					DEBUG_EMAIL(("%s\n", item->email->cc_address));
					break;
				case 0x0E04: // PR_DISPLAY_TO Address Sent-To
					DEBUG_EMAIL(("Display Sent-To Address - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->sentto_address, (char*));
					DEBUG_EMAIL(("%s\n", item->email->sentto_address));
					break;
				case 0x0E06: // PR_MESSAGE_DELIVERY_TIME Date 3 - Email Arrival Date
					DEBUG_EMAIL(("Date 3 (Delivery Time) - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->arrival_date, (FILETIME*));
					DEBUG_EMAIL(("%s", fileTimeToAscii(item->email->arrival_date)));
					break;
				case 0x0E07: // PR_MESSAGE_FLAGS Email Flag
					// 0x01 - Read
					// 0x02 - Unmodified
					// 0x04 - Submit
					// 0x08 - Unsent
					// 0x10 - Has Attachments
					// 0x20 - From Me
					// 0x40 - Associated
					// 0x80 - Resend
					// 0x100 - RN Pending
					// 0x200 - NRN Pending
					DEBUG_EMAIL(("Message Flags - "));
					MALLOC_EMAIL(item);
					memcpy(&(item->email->flag), list->items[x]->data, sizeof(item->email->flag));
					LE32_CPU(item->email->flag);
					DEBUG_EMAIL(("%i\n", item->email->flag));
					break;
				case 0x0E08: // PR_MESSAGE_SIZE Total size of a message object
					DEBUG_EMAIL(("Message Size - "));
					memcpy(&(item->message_size), list->items[x]->data, sizeof(item->message_size));
					LE32_CPU(item->message_size);
					DEBUG_EMAIL(("%i [%#x]\n", item->message_size, item->message_size));
					break;
				case 0x0E0A: // PR_SENTMAIL_ENTRYID
					// folder that this message is sent to after submission
					DEBUG_EMAIL(("Sentmail EntryID - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->sentmail_folder, (pst_entryid*));
					LE32_CPU(item->email->sentmail_folder->id);
					DEBUG_EMAIL(("[id = %#x]\n", item->email->sentmail_folder->id));
					break;
				case 0x0E1F: // PR_RTF_IN_SYNC
					// True means that the rtf version is same as text body
					// False means rtf version is more up-to-date than text body
					// if this value doesn't exist, text body is more up-to-date than rtf and
					//	 cannot update to the rtf
					DEBUG_EMAIL(("Compressed RTF in Sync - "));
					MALLOC_EMAIL(item);
					if (*(int16_t*)list->items[x]->data != 0) {
						DEBUG_EMAIL(("True\n"));
						item->email->rtf_in_sync = 1;
					} else {
						DEBUG_EMAIL(("False\n"));
						item->email->rtf_in_sync = 0;
					}
					break;
				case 0x0E20: // PR_ATTACH_SIZE binary Attachment data in record
					DEBUG_EMAIL(("Attachment Size - "));
					NULL_CHECK(attach);
					MOVE_NEXT(attach);
					memcpy(&(attach->size), list->items[x]->data, sizeof(attach->size));
					DEBUG_EMAIL(("%i\n", attach->size));
					break;
				case 0x0FF9: // PR_RECORD_KEY Record Header 1
					DEBUG_EMAIL(("Record Key 1 - "));
					LIST_COPY(item->record_key, (char*));
					item->record_key_size = list->items[x]->size;
					DEBUG_EMAIL_HEXPRINT(item->record_key, item->record_key_size);
					DEBUG_EMAIL(("\n"));
					break;
				case 0x1000: // PR_BODY Plain Text body
					DEBUG_EMAIL(("Plain Text body - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->body, (char*));
					//DEBUG_EMAIL("%s\n", item->email->body);
					DEBUG_EMAIL(("NOT PRINTED\n"));
					break;
				case 0x1006: // PR_RTF_SYNC_BODY_CRC
					DEBUG_EMAIL(("RTF Sync Body CRC - "));
					MALLOC_EMAIL(item);
					memcpy(&(item->email->rtf_body_crc), list->items[x]->data, sizeof(item->email->rtf_body_crc));
					LE32_CPU(item->email->rtf_body_crc);
					DEBUG_EMAIL(("%#x\n", item->email->rtf_body_crc));
					break;
				case 0x1007: // PR_RTF_SYNC_BODY_COUNT
					// a count of the *significant* charcters in the rtf body. Doesn't count
					// whitespace and other ignorable characters
					DEBUG_EMAIL(("RTF Sync Body character count - "));
					MALLOC_EMAIL(item);
					memcpy(&(item->email->rtf_body_char_count), list->items[x]->data, sizeof(item->email->rtf_body_char_count));
					LE32_CPU(item->email->rtf_body_char_count);
					DEBUG_EMAIL(("%i [%#x]\n", item->email->rtf_body_char_count, item->email->rtf_body_char_count));
					break;
				case 0x1008: // PR_RTF_SYNC_BODY_TAG
					// the first couple of lines of RTF body so that after modification, then beginning can
					// once again be found
					DEBUG_EMAIL(("RTF Sync body tag - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->rtf_body_tag, (char*));
					DEBUG_EMAIL(("%s\n", item->email->rtf_body_tag));
					break;
				case 0x1009: // PR_RTF_COMPRESSED
					// some compression algorithm has been applied to this. At present
					// it is unknown
					DEBUG_EMAIL(("RTF Compressed body - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->rtf_compressed, (char*));
					DEBUG_EMAIL(("NOT PRINTED\n"));
					break;
				case 0x1010: // PR_RTF_SYNC_PREFIX_COUNT
					// a count of the ignored characters before the first significant character
					DEBUG_EMAIL(("RTF whitespace prefix count - "));
					MALLOC_EMAIL(item);
					memcpy(&(item->email->rtf_ws_prefix_count), list->items[x]->data, sizeof(item->email->rtf_ws_prefix_count));
					DEBUG_EMAIL(("%i\n", item->email->rtf_ws_prefix_count));
					break;
				case 0x1011: // PR_RTF_SYNC_TRAILING_COUNT
					// a count of the ignored characters after the last significant character
					DEBUG_EMAIL(("RTF whitespace tailing count - "));
					MALLOC_EMAIL(item);
					memcpy(&(item->email->rtf_ws_trailing_count), list->items[x]->data, sizeof(item->email->rtf_ws_trailing_count));
					DEBUG_EMAIL(("%i\n", item->email->rtf_ws_trailing_count));
					break;
				case 0x1013: // HTML body
					DEBUG_EMAIL(("HTML body - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->htmlbody, (char*));
					//	DEBUG_EMAIL(("%s\n", item->email->htmlbody));
					DEBUG_EMAIL(("NOT PRINTED\n"));
					break;
				case 0x1035: // Message ID
					DEBUG_EMAIL(("Message ID - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->messageid, (char*));
					DEBUG_EMAIL(("%s\n", item->email->messageid));
					break;
				case 0x1042: // in-reply-to
					DEBUG_EMAIL(("In-Reply-To - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->in_reply_to, (char*));
					DEBUG_EMAIL(("%s\n", item->email->in_reply_to));
					break;
				case 0x1046: // Return Path
					DEBUG_EMAIL(("Return Path - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->return_path_address, (char*));
					DEBUG_EMAIL(("%s\n", item->email->return_path_address));
					break;
				case 0x3001: // PR_DISPLAY_NAME File As
					DEBUG_EMAIL(("Display Name - "));
					LIST_COPY(item->file_as, (char*));
					DEBUG_EMAIL(("%s\n", item->file_as));
					break;
				case 0x3002: // PR_ADDRTYPE
					DEBUG_EMAIL(("Address Type - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->address1_transport, (char*));
					DEBUG_EMAIL(("|%s|\n", item->contact->address1_transport));
					break;
				case 0x3003: // PR_EMAIL_ADDRESS
					// Contact's email address
					DEBUG_EMAIL(("Contact Address - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->address1, (char*));
					DEBUG_EMAIL(("|%s|\n", item->contact->address1));
					break;
				case 0x3004: // PR_COMMENT Comment for item - usually folders
					DEBUG_EMAIL(("Comment - "));
					LIST_COPY(item->comment, (char*));
					DEBUG_EMAIL(("%s\n", item->comment));
					break;
				case 0x3007: // PR_CREATION_TIME Date 4 - Creation Date?
					DEBUG_EMAIL(("Date 4 (Item Creation Date) - "));
					LIST_COPY(item->create_date, (FILETIME*));
					DEBUG_EMAIL(("%s", fileTimeToAscii(item->create_date)));
					break;
				case 0x3008: // PR_LAST_MODIFICATION_TIME Date 5 - Modify Date
					DEBUG_EMAIL(("Date 5 (Modify Date) - "));
					LIST_COPY(item->modify_date, (FILETIME*));
					DEBUG_EMAIL(("%s", fileTimeToAscii(item->modify_date)));
					break;
				case 0x300B: // PR_SEARCH_KEY Record Header 2
					DEBUG_EMAIL(("Record Search 2 -- NOT HANDLED\n"));
					break;
				case 0x35DF: // PR_VALID_FOLDER_MASK
					// States which folders are valid for this message store
					// FOLDER_IPM_SUBTREE_VALID 0x1
					// FOLDER_IPM_INBOX_VALID	0x2
					// FOLDER_IPM_OUTBOX_VALID	0x4
					// FOLDER_IPM_WASTEBOX_VALID 0x8
					// FOLDER_IPM_SENTMAIL_VALID 0x10
					// FOLDER_VIEWS_VALID		 0x20
					// FOLDER_COMMON_VIEWS_VALID 0x40
					// FOLDER_FINDER_VALID		 0x80
					DEBUG_EMAIL(("Valid Folder Mask - "));
					MALLOC_MESSAGESTORE(item);
					memcpy(&(item->message_store->valid_mask), list->items[x]->data, sizeof(int));
					LE32_CPU(item->message_store->valid_mask);
					DEBUG_EMAIL(("%i\n", item->message_store->valid_mask));
					break;
				case 0x35E0: // PR_IPM_SUBTREE_ENTRYID Top of Personal Folder Record
					DEBUG_EMAIL(("Top of Personal Folder Record - "));
					MALLOC_MESSAGESTORE(item);
					LIST_COPY(item->message_store->top_of_personal_folder, (pst_entryid*));
					LE32_CPU(item->message_store->top_of_personal_folder->id);
					DEBUG_EMAIL(("[id = %#x]\n", item->message_store->top_of_personal_folder->id));
					break;
				case 0x35E3: // PR_IPM_WASTEBASKET_ENTRYID Deleted Items Folder Record
					DEBUG_EMAIL(("Deleted Items Folder record - "));
					MALLOC_MESSAGESTORE(item);
					LIST_COPY(item->message_store->deleted_items_folder, (pst_entryid*));
					LE32_CPU(item->message_store->deleted_items_folder->id);
					DEBUG_EMAIL(("[id = %#x]\n", item->message_store->deleted_items_folder->id));
					break;
				case 0x35E7: // PR_FINDER_ENTRYID Search Root Record
					DEBUG_EMAIL(("Search Root record - "));
					MALLOC_MESSAGESTORE(item);
					LIST_COPY(item->message_store->search_root_folder, (pst_entryid*));
					LE32_CPU(item->message_store->search_root_folder->id);
					DEBUG_EMAIL(("[id = %#x]\n", item->message_store->search_root_folder->id));
					break;
				case 0x3602: // PR_CONTENT_COUNT Number of emails stored in a folder
					DEBUG_EMAIL(("Folder Email Count - "));
					MALLOC_FOLDER(item);
					memcpy(&(item->folder->email_count), list->items[x]->data, sizeof(item->folder->email_count));
					LE32_CPU(item->folder->email_count);
					DEBUG_EMAIL(("%i\n", item->folder->email_count));
					break;
				case 0x3603: // PR_CONTENT_UNREAD Number of unread emails
					DEBUG_EMAIL(("Unread Email Count - "));
					MALLOC_FOLDER(item);
					memcpy(&(item->folder->unseen_email_count), list->items[x]->data, sizeof(item->folder->unseen_email_count));
					LE32_CPU(item->folder->unseen_email_count);
					DEBUG_EMAIL(("%i\n", item->folder->unseen_email_count));
					break;
				case 0x360A: // PR_SUBFOLDERS Has children
					DEBUG_EMAIL(("Has Subfolders - "));
					MALLOC_FOLDER(item);
					if (*((int32_t*)list->items[x]->data) != 0) {
						DEBUG_EMAIL(("True\n"));
						item->folder->subfolder = 1;
					} else {
						DEBUG_EMAIL(("False\n"));
						item->folder->subfolder = 0;
					}
					break;
				case 0x3613: // PR_CONTAINER_CLASS IPF.x
					DEBUG_EMAIL(("IPF.x - "));
					LIST_COPY(item->ascii_type, (char*));
					if (strncmp("IPF.Note", item->ascii_type, 8) == 0)
						item->type = PST_TYPE_NOTE;
					else if (strncmp("IPF.Contact", item->ascii_type, 11) == 0)
						item->type = PST_TYPE_CONTACT;
					else if (strncmp("IPF.Journal", item->ascii_type, 11) == 0)
						item->type = PST_TYPE_JOURNAL;
					else if (strncmp("IPF.Appointment", item->ascii_type, 15) == 0)
						item->type = PST_TYPE_APPOINTMENT;
					else if (strncmp("IPF.StickyNote", item->ascii_type, 14) == 0)
						item->type = PST_TYPE_STICKYNOTE;
					else if (strncmp("IPF.Task", item->ascii_type, 8) == 0)
						item->type = PST_TYPE_TASK;
					else
						item->type = PST_TYPE_OTHER;

					DEBUG_EMAIL(("%s [%i]\n", item->ascii_type, item->type));
					break;
				case 0x3617: // PR_ASSOC_CONTENT_COUNT
					// associated content are items that are attached to this folder
					// but are hidden from users
					DEBUG_EMAIL(("Associate Content count - "));
					MALLOC_FOLDER(item);
					memcpy(&(item->folder->assoc_count), list->items[x]->data, sizeof(item->folder->assoc_count));
					LE32_CPU(item->folder->assoc_count);
					DEBUG_EMAIL(("%i [%#x]\n", item->folder->assoc_count, item->folder->assoc_count));
					break;
				case 0x3701: // PR_ATTACH_DATA_OBJ binary data of attachment
					DEBUG_EMAIL(("Binary Data [Size %i] - ", list->items[x]->size));
					NULL_CHECK(attach);
					MOVE_NEXT(attach);
					if (!list->items[x]->data) { //special case
						attach->id2_val = list->items[x]->type;
						DEBUG_EMAIL(("Seen a Reference. The data hasn't been loaded yet. [%#x][%#x]\n",
								 attach->id2_val, list->items[x]->type));
					} else {
						LIST_COPY(attach->data, (char*));
						attach->size = list->items[x]->size;
						DEBUG_EMAIL(("NOT PRINTED\n"));
					}
					break;
				case 0x3704: // PR_ATTACH_FILENAME Attachment filename (8.3)
					DEBUG_EMAIL(("Attachment Filename - "));
					NULL_CHECK(attach);
					MOVE_NEXT(attach);
					LIST_COPY(attach->filename1, (char*));
					DEBUG_EMAIL(("%s\n", attach->filename1));
					break;
				case 0x3705: // PR_ATTACH_METHOD
					// 0 - No Attachment
					// 1 - Attach by Value
					// 2 - Attach by reference
					// 3 - Attach by ref resolve
					// 4 - Attach by ref only
					// 5 - Embedded Message
					// 6 - OLE
					DEBUG_EMAIL(("Attachement method - "));
					NULL_CHECK(attach);
					MOVE_NEXT(attach);
					memcpy(&(attach->method), list->items[x]->data, sizeof(attach->method));
					LE32_CPU(attach->method);
					t = attach->method;
					DEBUG_EMAIL(("%s [%i]\n", (t==0?"No Attachment":
								   (t==1?"Attach By Value":
									(t==2?"Attach By Reference":
									 (t==3?"Attach by Ref. Resolve":
									  (t==4?"Attach by Ref. Only":
									   (t==5?"Embedded Message":"OLE")))))),t));
					break;
				case 0x3707: // PR_ATTACH_LONG_FILENAME Attachment filename (long?)
					DEBUG_EMAIL(("Attachment Filename long - "));
					NULL_CHECK(attach);
					MOVE_NEXT(attach);
					LIST_COPY(attach->filename2, (char*));
					DEBUG_EMAIL(("%s\n", attach->filename2));
					break;
				case 0x370B: // PR_RENDERING_POSITION
					// position in characters that the attachment appears in the plain text body
					DEBUG_EMAIL(("Attachment Position - "));
					NULL_CHECK(attach);
					MOVE_NEXT(attach);
					memcpy(&(attach->position), list->items[x]->data, sizeof(attach->position));
					LE32_CPU(attach->position);
					DEBUG_EMAIL(("%i [%#x]\n", attach->position));
					break;
				case 0x370E: // PR_ATTACH_MIME_TAG Mime type of encoding
					DEBUG_EMAIL(("Attachment mime encoding - "));
					NULL_CHECK(attach);
					MOVE_NEXT(attach);
					LIST_COPY(attach->mimetype, (char*));
					DEBUG_EMAIL(("%s\n", attach->mimetype));
					break;
				case 0x3710: // PR_ATTACH_MIME_SEQUENCE
					// sequence number for mime parts. Includes body
					DEBUG_EMAIL(("Attachment Mime Sequence - "));
					NULL_CHECK(attach);
					MOVE_NEXT(attach);
					memcpy(&(attach->sequence), list->items[x]->data, sizeof(attach->sequence));
					LE32_CPU(attach->sequence);
					DEBUG_EMAIL(("%i\n", attach->sequence));
					break;
				case 0x3A00: // PR_ACCOUNT
					DEBUG_EMAIL(("Contact's Account name - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->account_name, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->account_name));
					break;
				case 0x3A01: // PR_ALTERNATE_RECIPIENT
					DEBUG_EMAIL(("Contact Alternate Recipient - NOT PROCESSED\n"));
					break;
				case 0x3A02: // PR_CALLBACK_TELEPHONE_NUMBER
					DEBUG_EMAIL(("Callback telephone number - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->callback_phone, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->callback_phone));
					break;
				case 0x3A03: // PR_CONVERSION_PROHIBITED
					DEBUG_EMAIL(("Message Conversion Prohibited - "));
					MALLOC_EMAIL(item);
					if (*(int16_t*)list->items[x]->data != 0) {
						DEBUG_EMAIL(("True\n"));
						item->email->conversion_prohib = 1;
					} else {
						DEBUG_EMAIL(("False\n"));
						item->email->conversion_prohib = 0;
					}
					break;
				case 0x3A05: // PR_GENERATION suffix
					DEBUG_EMAIL(("Contacts Suffix - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->suffix, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->suffix));
					break;
				case 0x3A06: // PR_GIVEN_NAME Contact's first name
					DEBUG_EMAIL(("Contacts First Name - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->first_name, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->first_name));
					break;
				case 0x3A07: // PR_GOVERNMENT_ID_NUMBER
					DEBUG_EMAIL(("Contacts Government ID Number - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->gov_id, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->gov_id));
					break;
				case 0x3A08: // PR_BUSINESS_TELEPHONE_NUMBER
					DEBUG_EMAIL(("Business Telephone Number - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->business_phone, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->business_phone));
					break;
				case 0x3A09: // PR_HOME_TELEPHONE_NUMBER
					DEBUG_EMAIL(("Home Telephone Number - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->home_phone, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->home_phone));
					break;
				case 0x3A0A: // PR_INITIALS Contact's Initials
					DEBUG_EMAIL(("Contacts Initials - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->initials, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->initials));
					break;
				case 0x3A0B: // PR_KEYWORD
					DEBUG_EMAIL(("Keyword - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->keyword, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->keyword));
					break;
				case 0x3A0C: // PR_LANGUAGE
					DEBUG_EMAIL(("Contact's Language - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->language, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->language));
					break;
				case 0x3A0D: // PR_LOCATION
					DEBUG_EMAIL(("Contact's Location - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->location, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->location));
					break;
				case 0x3A0E: // PR_MAIL_PERMISSION - Can the recipient receive and send email
					DEBUG_EMAIL(("Mail Permission - "));
					MALLOC_CONTACT(item);
					if (*(int16_t*)list->items[x]->data != 0) {
						DEBUG_EMAIL(("True\n"));
						item->contact->mail_permission = 1;
					} else {
						DEBUG_EMAIL(("False\n"));
						item->contact->mail_permission = 0;
					}
					break;
				case 0x3A0F: // PR_MHS_COMMON_NAME
					DEBUG_EMAIL(("MHS Common Name - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->common_name, (char*));
					DEBUG_EMAIL(("%s\n", item->email->common_name));
					break;
				case 0x3A10: // PR_ORGANIZATIONAL_ID_NUMBER
					DEBUG_EMAIL(("Organizational ID # - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->org_id, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->org_id));
					break;
				case 0x3A11: // PR_SURNAME Contact's Surname
					DEBUG_EMAIL(("Contacts Surname - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->surname, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->surname));
					break;
				case 0x3A12: // PR_ORIGINAL_ENTRY_ID
					DEBUG_EMAIL(("Original Entry ID - NOT PROCESSED\n"));
					break;
				case 0x3A13: // PR_ORIGINAL_DISPLAY_NAME
					DEBUG_EMAIL(("Original Display Name - NOT PROCESSED\n"));
					break;
				case 0x3A14: // PR_ORIGINAL_SEARCH_KEY
					DEBUG_EMAIL(("Original Search Key - NOT PROCESSED\n"));
					break;
				case 0x3A15: // PR_POSTAL_ADDRESS
					DEBUG_EMAIL(("Default Postal Address - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->def_postal_address, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->def_postal_address));
					break;
				case 0x3A16: // PR_COMPANY_NAME
					DEBUG_EMAIL(("Company Name - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->company_name, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->company_name));
					break;
				case 0x3A17: // PR_TITLE - Job Title
					DEBUG_EMAIL(("Job Title - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->job_title, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->job_title));
					break;
				case 0x3A18: // PR_DEPARTMENT_NAME
					DEBUG_EMAIL(("Department Name - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->department, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->department));
					break;
				case 0x3A19: // PR_OFFICE_LOCATION
					DEBUG_EMAIL(("Office Location - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->office_loc, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->office_loc));
					break;
				case 0x3A1A: // PR_PRIMARY_TELEPHONE_NUMBER
					DEBUG_EMAIL(("Primary Telephone - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->primary_phone, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->primary_phone));
					break;
				case 0x3A1B: // PR_BUSINESS2_TELEPHONE_NUMBER
					DEBUG_EMAIL(("Business Phone Number 2 - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->business_phone2, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->business_phone2));
					break;
				case 0x3A1C: // PR_MOBILE_TELEPHONE_NUMBER
					DEBUG_EMAIL(("Mobile Phone Number - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->mobile_phone, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->mobile_phone));
					break;
				case 0x3A1D: // PR_RADIO_TELEPHONE_NUMBER
					DEBUG_EMAIL(("Radio Phone Number - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->radio_phone, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->radio_phone));
					break;
				case 0x3A1E: // PR_CAR_TELEPHONE_NUMBER
					DEBUG_EMAIL(("Car Phone Number - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->car_phone, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->car_phone));
					break;
				case 0x3A1F: // PR_OTHER_TELEPHONE_NUMBER
					DEBUG_EMAIL(("Other Phone Number - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->other_phone, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->other_phone));
					break;
				case 0x3A20: // PR_TRANSMITTABLE_DISPLAY_NAME
					DEBUG_EMAIL(("Transmittable Display Name - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->transmittable_display_name, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->transmittable_display_name));
					break;
				case 0x3A21: // PR_PAGER_TELEPHONE_NUMBER
					DEBUG_EMAIL(("Pager Phone Number - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->pager_phone, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->pager_phone));
					break;
				case 0x3A22: // PR_USER_CERTIFICATE
					DEBUG_EMAIL(("User Certificate - NOT PROCESSED"));
					break;
				case 0x3A23: // PR_PRIMARY_FAX_NUMBER
					DEBUG_EMAIL(("Primary Fax Number - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->primary_fax, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->primary_fax));
					break;
				case 0x3A24: // PR_BUSINESS_FAX_NUMBER
					DEBUG_EMAIL(("Business Fax Number - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->business_fax, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->business_fax));
					break;
				case 0x3A25: // PR_HOME_FAX_NUMBER
					DEBUG_EMAIL(("Home Fax Number - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->home_fax, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->home_fax));
					break;
				case 0x3A26: // PR_BUSINESS_ADDRESS_COUNTRY
					DEBUG_EMAIL(("Business Address Country - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->business_country, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->business_country));
					break;
				case 0x3A27: // PR_BUSINESS_ADDRESS_CITY
					DEBUG_EMAIL(("Business Address City - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->business_city, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->business_city));
					break;
				case 0x3A28: // PR_BUSINESS_ADDRESS_STATE_OR_PROVINCE
					DEBUG_EMAIL(("Business Address State - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->business_state, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->business_state));
					break;
				case 0x3A29: // PR_BUSINESS_ADDRESS_STREET
					DEBUG_EMAIL(("Business Address Street - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->business_street, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->business_street));
					break;
				case 0x3A2A: // PR_BUSINESS_POSTAL_CODE
					DEBUG_EMAIL(("Business Postal Code - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->business_postal_code, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->business_postal_code));
					break;
				case 0x3A2B: // PR_BUSINESS_PO_BOX
					DEBUG_EMAIL(("Business PO Box - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->business_po_box, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->business_po_box));
					break;
				case 0x3A2C: // PR_TELEX_NUMBER
					DEBUG_EMAIL(("Telex Number - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->telex, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->telex));
					break;
				case 0x3A2D: // PR_ISDN_NUMBER
					DEBUG_EMAIL(("ISDN Number - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->isdn_phone, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->isdn_phone));
					break;
				case 0x3A2E: // PR_ASSISTANT_TELEPHONE_NUMBER
					DEBUG_EMAIL(("Assistant Phone Number - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->assistant_phone, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->assistant_phone));
					break;
				case 0x3A2F: // PR_HOME2_TELEPHONE_NUMBER
					DEBUG_EMAIL(("Home Phone 2 - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->home_phone2, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->home_phone2));
					break;
				case 0x3A30: // PR_ASSISTANT
					DEBUG_EMAIL(("Assistant's Name - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->assistant_name, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->assistant_name));
					break;
				case 0x3A40: // PR_SEND_RICH_INFO
					DEBUG_EMAIL(("Can receive Rich Text - "));
					MALLOC_CONTACT(item);
					if(*(int16_t*)list->items[x]->data != 0) {
						DEBUG_EMAIL(("True\n"));
						item->contact->rich_text = 1;
					} else {
						DEBUG_EMAIL(("False\n"));
						item->contact->rich_text = 0;
					}
					break;
				case 0x3A41: // PR_WEDDING_ANNIVERSARY
					DEBUG_EMAIL(("Wedding Anniversary - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->wedding_anniversary, (FILETIME*));
					DEBUG_EMAIL(("%s\n", fileTimeToAscii(item->contact->wedding_anniversary)));
					break;
				case 0x3A42: // PR_BIRTHDAY
					DEBUG_EMAIL(("Birthday - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->birthday, (FILETIME*));
					DEBUG_EMAIL(("%s\n", fileTimeToAscii(item->contact->birthday)));
					break;
				case 0x3A43: // PR_HOBBIES
					DEBUG_EMAIL(("Hobbies - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->hobbies, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->hobbies));
					break;
				case 0x3A44: // PR_MIDDLE_NAME
					DEBUG_EMAIL(("Middle Name - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->middle_name, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->middle_name));
					break;
				case 0x3A45: // PR_DISPLAY_NAME_PREFIX
					DEBUG_EMAIL(("Display Name Prefix (Title) - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->display_name_prefix, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->display_name_prefix));
					break;
				case 0x3A46: // PR_PROFESSION
					DEBUG_EMAIL(("Profession - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->profession, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->profession));
					break;
				case 0x3A47: // PR_PREFERRED_BY_NAME
					DEBUG_EMAIL(("Preferred By Name - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->pref_name, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->pref_name));
					break;
				case 0x3A48: // PR_SPOUSE_NAME
					DEBUG_EMAIL(("Spouse's Name - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->spouse_name, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->spouse_name));
					break;
				case 0x3A49: // PR_COMPUTER_NETWORK_NAME
					DEBUG_EMAIL(("Computer Network Name - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->computer_name, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->computer_name));
					break;
				case 0x3A4A: // PR_CUSTOMER_ID
					DEBUG_EMAIL(("Customer ID - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->customer_id, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->customer_id));
					break;
				case 0x3A4B: // PR_TTYTDD_PHONE_NUMBER
					DEBUG_EMAIL(("TTY/TDD Phone - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->ttytdd_phone, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->ttytdd_phone));
					break;
				case 0x3A4C: // PR_FTP_SITE
					DEBUG_EMAIL(("Ftp Site - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->ftp_site, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->ftp_site));
					break;
				case 0x3A4D: // PR_GENDER
					DEBUG_EMAIL(("Gender - "));
					MALLOC_CONTACT(item);
					memcpy(&item->contact->gender, list->items[x]->data, sizeof(int16_t));
					LE16_CPU(item->contact->gender);
					switch(item->contact->gender) {
						case 0:
							DEBUG_EMAIL(("Unspecified\n"));
							break;
						case 1:
							DEBUG_EMAIL(("Female\n"));
							break;
						case 2:
							DEBUG_EMAIL(("Male\n"));
							break;
						default:
							DEBUG_EMAIL(("Error processing\n"));
					}
					break;
				case 0x3A4E: // PR_MANAGER_NAME
					DEBUG_EMAIL(("Manager's Name - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->manager_name, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->manager_name));
					break;
				case 0x3A4F: // PR_NICKNAME
					DEBUG_EMAIL(("Nickname - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->nickname, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->nickname));
					break;
				case 0x3A50: // PR_PERSONAL_HOME_PAGE
					DEBUG_EMAIL(("Personal Home Page - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->personal_homepage, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->personal_homepage));
					break;
				case 0x3A51: // PR_BUSINESS_HOME_PAGE
					DEBUG_EMAIL(("Business Home Page - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->business_homepage, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->business_homepage));
					break;
				case 0x3A57: // PR_COMPANY_MAIN_PHONE_NUMBER
					DEBUG_EMAIL(("Company Main Phone - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->company_main_phone, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->company_main_phone));
					break;
				case 0x3A58: // PR_CHILDRENS_NAMES
					DEBUG_EMAIL(("Children's Names - NOT PROCESSED\n"));
					break;
				case 0x3A59: // PR_HOME_ADDRESS_CITY
					DEBUG_EMAIL(("Home Address City - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->home_city, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->home_city));
					break;
				case 0x3A5A: // PR_HOME_ADDRESS_COUNTRY
					DEBUG_EMAIL(("Home Address Country - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->home_country, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->home_country));
					break;
				case 0x3A5B: // PR_HOME_ADDRESS_POSTAL_CODE
					DEBUG_EMAIL(("Home Address Postal Code - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->home_postal_code, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->home_postal_code));
					break;
				case 0x3A5C: // PR_HOME_ADDRESS_STATE_OR_PROVINCE
					DEBUG_EMAIL(("Home Address State or Province - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->home_state, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->home_state));
					break;
				case 0x3A5D: // PR_HOME_ADDRESS_STREET
					DEBUG_EMAIL(("Home Address Street - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->home_street, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->home_street));
					break;
				case 0x3A5E: // PR_HOME_ADDRESS_POST_OFFICE_BOX
					DEBUG_EMAIL(("Home Address Post Office Box - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->home_po_box, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->home_po_box));
					break;
				case 0x3A5F: // PR_OTHER_ADDRESS_CITY
					DEBUG_EMAIL(("Other Address City - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->other_city, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->other_city));
					break;
				case 0x3A60: // PR_OTHER_ADDRESS_COUNTRY
					DEBUG_EMAIL(("Other Address Country - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->other_country, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->other_country));
					break;
				case 0x3A61: // PR_OTHER_ADDRESS_POSTAL_CODE
					DEBUG_EMAIL(("Other Address Postal Code - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->other_postal_code, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->other_postal_code));
					break;
				case 0x3A62: // PR_OTHER_ADDRESS_STATE_OR_PROVINCE
					DEBUG_EMAIL(("Other Address State - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->other_state, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->other_state));
					break;
				case 0x3A63: // PR_OTHER_ADDRESS_STREET
					DEBUG_EMAIL(("Other Address Street - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->other_street, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->other_street));
					break;
				case 0x3A64: // PR_OTHER_ADDRESS_POST_OFFICE_BOX
					DEBUG_EMAIL(("Other Address Post Office box - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->other_po_box, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->other_po_box));
					break;
				case 0x65E3: // Entry ID?
					DEBUG_EMAIL(("Entry ID - "));
					item->record_key = (char*) xmalloc(16+1);
					memcpy(item->record_key, &(list->items[x]->data[1]), 16); //skip first byte
					item->record_key[16]='\0';
					item->record_key_size=16;
					DEBUG_EMAIL_HEXPRINT((char*)item->record_key, 16);
					break;
				case 0x67F2: // ID2 value of the attachments proper record
					DEBUG_EMAIL(("Attachment ID2 value - "));
					if (attach){
						MOVE_NEXT(attach);
						memcpy(&(attach->id2_val), list->items[x]->data, sizeof(attach->id2_val));
						LE32_CPU(attach->id2_val);
						DEBUG_EMAIL(("%#x\n", attach->id2_val));
					} else {
						DEBUG_EMAIL(("NOT AN ATTACHMENT: %#x\n", list->items[x]->id));
					}
					break;
				case 0x67FF: // Extra Property Identifier (Password CheckSum)
					DEBUG_EMAIL(("Password checksum [0x67FF] - "));
					MALLOC_MESSAGESTORE(item);
					memcpy(&(item->message_store->pwd_chksum), list->items[x]->data,
						   sizeof(item->message_store->pwd_chksum));
					DEBUG_EMAIL(("%#x\n", item->message_store->pwd_chksum));
					break;
				case 0x6F02: // Secure HTML Body
					DEBUG_EMAIL(("Secure HTML Body - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->encrypted_htmlbody, (char*));
					item->email->encrypted_htmlbody_size = list->items[x]->size;
					DEBUG_EMAIL(("Not Printed\n"));
					break;
				case 0x6F04: // Secure Text Body
					DEBUG_EMAIL(("Secure Text Body - "));
					MALLOC_EMAIL(item);
					LIST_COPY(item->email->encrypted_body, (char*));
					item->email->encrypted_body_size = list->items[x]->size;
					DEBUG_EMAIL(("Not Printed\n"));
					break;
				case 0x7C07: // top of folders ENTRYID
					DEBUG_EMAIL(("Top of folders RecID [0x7c07] - "));
					MALLOC_MESSAGESTORE(item);
					item->message_store->top_of_folder = (pst_entryid*) xmalloc(sizeof(pst_entryid));
					memcpy(item->message_store->top_of_folder, list->items[x]->data, sizeof(pst_entryid));
					LE32_CPU(item->message_store->top_of_folder->u1);
					LE32_CPU(item->message_store->top_of_folder->id);
					DEBUG_EMAIL_HEXPRINT((char*)item->message_store->top_of_folder->entryid, 16);
					break;
				case 0x8005: // Contact's Fullname
					DEBUG_EMAIL(("Contact Fullname - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->fullname, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->fullname));
					break;
				case 0x801A: // Full Home Address
					DEBUG_EMAIL(("Home Address - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->home_address, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->home_address));
					break;
				case 0x801B: // Full Business Address
					DEBUG_EMAIL(("Business Address - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->business_address, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->business_address));
					break;
				case 0x801C: // Full Other Address
					DEBUG_EMAIL(("Other Address - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->other_address, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->other_address));
					break;
				case 0x8082: // Email Address 1 Transport
					DEBUG_EMAIL(("Email Address 1 Transport - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->address1_transport, (char*));
					DEBUG_EMAIL(("|%s|\n", item->contact->address1_transport));
					break;
				case 0x8083: // Email Address 1 Address
					DEBUG_EMAIL(("Email Address 1 Address - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->address1, (char*));
					DEBUG_EMAIL(("|%s|\n", item->contact->address1));
					break;
				case 0x8084: // Email Address 1 Description
					DEBUG_EMAIL(("Email Address 1 Description - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->address1_desc, (char*));
					DEBUG_EMAIL(("|%s|\n", item->contact->address1_desc));
					break;
				case 0x8085: // Email Address 1 Record
					DEBUG_EMAIL(("Email Address 1 Record - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->address1a, (char*));
					DEBUG_EMAIL(("|%s|\n", item->contact->address1a));
					break;
				case 0x8092: // Email Address 2 Transport
					DEBUG_EMAIL(("Email Address 2 Transport - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->address2_transport, (char*));
					DEBUG_EMAIL(("|%s|\n", item->contact->address2_transport));
					break;
				case 0x8093: // Email Address 2 Address
					DEBUG_EMAIL(("Email Address 2 Address - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->address2, (char*));
					DEBUG_EMAIL(("|%s|\n", item->contact->address2));
					break;
				case 0x8094: // Email Address 2 Description
					DEBUG_EMAIL (("Email Address 2 Description - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->address2_desc, (char*));
					DEBUG_EMAIL(("|%s|\n", item->contact->address2_desc));
					break;
				case 0x8095: // Email Address 2 Record
					DEBUG_EMAIL(("Email Address 2 Record - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->address2a, (char*));
					DEBUG_EMAIL(("|%s|\n", item->contact->address2a));
					break;
				case 0x80A2: // Email Address 3 Transport
					DEBUG_EMAIL (("Email Address 3 Transport - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->address3_transport, (char*));
					DEBUG_EMAIL(("|%s|\n", item->contact->address3_transport));
					break;
				case 0x80A3: // Email Address 3 Address
					DEBUG_EMAIL(("Email Address 3 Address - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->address3, (char*));
					DEBUG_EMAIL(("|%s|\n", item->contact->address3));
					break;
				case 0x80A4: // Email Address 3 Description
					DEBUG_EMAIL(("Email Address 3 Description - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->address3_desc, (char*));
					DEBUG_EMAIL(("|%s|\n", item->contact->address3_desc));
					break;
				case 0x80A5: // Email Address 3 Record
					DEBUG_EMAIL(("Email Address 3 Record - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->address3a, (char*));
					DEBUG_EMAIL(("|%s|\n", item->contact->address3a));
					break;
				case 0x80D8: // Internet Free/Busy
					DEBUG_EMAIL(("Internet Free/Busy - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->free_busy_address, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->free_busy_address));
					break;
				case 0x8205: // Show on Free/Busy as
					// 0: Free
					// 1: Tentative
					// 2: Busy
					// 3: Out Of Office
					DEBUG_EMAIL(("Appointment shows as - "));
					MALLOC_APPOINTMENT(item);
					memcpy(&(item->appointment->showas), list->items[x]->data, sizeof(item->appointment->showas));
					LE32_CPU(item->appointment->showas);
					switch (item->appointment->showas) {
						case PST_FREEBUSY_FREE:
							DEBUG_EMAIL(("Free\n")); break;
						case PST_FREEBUSY_TENTATIVE:
							DEBUG_EMAIL(("Tentative\n")); break;
						case PST_FREEBUSY_BUSY:
							DEBUG_EMAIL(("Busy\n")); break;
						case PST_FREEBUSY_OUT_OF_OFFICE:
							DEBUG_EMAIL(("Out Of Office\n")); break;
						default:
							DEBUG_EMAIL(("Unknown Value: %d\n", item->appointment->showas)); break;
					}
					break;
				case 0x8208: // Location of an appointment
					DEBUG_EMAIL(("Appointment Location - "));
					MALLOC_APPOINTMENT(item);
					LIST_COPY(item->appointment->location, (char*));
					DEBUG_EMAIL(("%s\n", item->appointment->location));
					break;
				case 0x8214: // Label for an appointment
					DEBUG_EMAIL(("Label for appointment - "));
					MALLOC_APPOINTMENT(item);
					memcpy(&(item->appointment->label), list->items[x]->data, sizeof(item->appointment->label));
					LE32_CPU(item->appointment->label);
					switch (item->appointment->label) {
						case PST_APP_LABEL_NONE:
							DEBUG_EMAIL(("None\n")); break;
						case PST_APP_LABEL_IMPORTANT:
							DEBUG_EMAIL(("Important\n")); break;
						case PST_APP_LABEL_BUSINESS:
							DEBUG_EMAIL(("Business\n")); break;
						case PST_APP_LABEL_PERSONAL:
							DEBUG_EMAIL(("Personal\n")); break;
						case PST_APP_LABEL_VACATION:
							DEBUG_EMAIL(("Vacation\n")); break;
						case PST_APP_LABEL_MUST_ATTEND:
							DEBUG_EMAIL(("Must Attend\n")); break;
						case PST_APP_LABEL_TRAVEL_REQ:
							DEBUG_EMAIL(("Travel Required\n")); break;
						case PST_APP_LABEL_NEEDS_PREP:
							DEBUG_EMAIL(("Needs Preparation\n")); break;
						case PST_APP_LABEL_BIRTHDAY:
							DEBUG_EMAIL(("Birthday\n")); break;
						case PST_APP_LABEL_ANNIVERSARY:
							DEBUG_EMAIL(("Anniversary\n")); break;
						case PST_APP_LABEL_PHONE_CALL:
							DEBUG_EMAIL(("Phone Call\n")); break;
					}
					break;
				case 0x8215: // All day appointment flag
					DEBUG_EMAIL(("All day flag - "));
					MALLOC_APPOINTMENT(item);
					if (*(int16_t*)list->items[x]->data != 0) {
						DEBUG_EMAIL(("True\n"));
						item->appointment->all_day = 1;
					} else {
						DEBUG_EMAIL(("False\n"));
						item->appointment->all_day = 0;
					}
					break;
				case 0x8234: // TimeZone as String
					DEBUG_EMAIL(("TimeZone of times - "));
					MALLOC_APPOINTMENT(item);
					LIST_COPY(item->appointment->timezonestring, (char*));
					DEBUG_EMAIL(("%s\n", item->appointment->timezonestring));
					break;
				case 0x8235: // Appointment start time
					DEBUG_EMAIL(("Appointment Start Time - "));
					MALLOC_APPOINTMENT(item);
					LIST_COPY(item->appointment->start, (FILETIME*));
					DEBUG_EMAIL(("%s\n", fileTimeToAscii((FILETIME*)item->appointment->start)));
					break;
				case 0x8236: // Appointment end time
					DEBUG_EMAIL(("Appointment End Time - "));
					MALLOC_APPOINTMENT(item);
					LIST_COPY(item->appointment->end, (FILETIME*));
					DEBUG_EMAIL(("%s\n", fileTimeToAscii((FILETIME*)item->appointment->start)));
					break;
				case 0x8516: // Journal time start
					DEBUG_EMAIL(("Duplicate Time Start - "));
					DEBUG_EMAIL(("%s\n", fileTimeToAscii((FILETIME*)list->items[x]->data)));
					break;
				case 0x8517: // Journal time end
					DEBUG_EMAIL(("Duplicate Time End - "));
					DEBUG_EMAIL(("%s\n", fileTimeToAscii((FILETIME*)list->items[x]->data)));
					break;
				case 0x8530: // Followup
					DEBUG_EMAIL(("Followup String - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->followup, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->followup));
					break;
				case 0x8534: // Mileage
					DEBUG_EMAIL(("Mileage - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->mileage, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->mileage));
					break;
				case 0x8535: // Billing Information
					DEBUG_EMAIL(("Billing Information - "));
					MALLOC_CONTACT(item);
					LIST_COPY(item->contact->billing_information, (char*));
					DEBUG_EMAIL(("%s\n", item->contact->billing_information));
					break;
				case 0x8554: // Outlook Version
					DEBUG_EMAIL(("Outlook Version - "));
					LIST_COPY(item->outlook_version, (char*));
					DEBUG_EMAIL(("%s\n", item->outlook_version));
					break;
				case 0x8560: // Appointment Reminder Time
					DEBUG_EMAIL(("Appointment Reminder Time - "));
					MALLOC_APPOINTMENT(item);
					LIST_COPY(item->appointment->reminder, (FILETIME*));
					DEBUG_EMAIL(("%s\n", fileTimeToAscii(item->appointment->reminder)));
					break;
				case 0x8700: // Journal Type
					DEBUG_EMAIL(("Journal Entry Type - "));
					MALLOC_JOURNAL(item);
					LIST_COPY(item->journal->type, (char*));
					DEBUG_EMAIL(("%s\n", item->journal->type));
					break;
				case 0x8706: // Journal Start date/time
					DEBUG_EMAIL(("Start Timestamp - "));
					MALLOC_JOURNAL(item);
					LIST_COPY(item->journal->start, (FILETIME*));
					DEBUG_EMAIL(("%s\n", fileTimeToAscii(item->journal->start)));
					break;
				case 0x8708: // Journal End date/time
					DEBUG_EMAIL(("End Timestamp - "));
					MALLOC_JOURNAL(item);
					LIST_COPY(item->journal->end, (FILETIME*));
					DEBUG_EMAIL(("%s\n", fileTimeToAscii(item->journal->end)));
					break;
				case 0x8712: // Title?
					DEBUG_EMAIL(("Journal Entry Type - "));
					MALLOC_JOURNAL(item);
					LIST_COPY(item->journal->type, (char*));
					DEBUG_EMAIL(("%s\n", item->journal->type));
					break;
				default:
					DEBUG_EMAIL(("unknown type %#x\n", list->items[x]->id));
						/* Reference Types

					   2 - 0x0002 - Signed 16bit value
					   3 - 0x0003 - Signed 32bit value
					  11 - 0x000B - Boolean (non-zero = true)
					  13 - 0x000D - Embedded Object
					  30 - 0x001E - Null terminated String
					  31 - 0x001F - Unicode string
					  64 - 0x0040 - Systime - Filetime structure
					  72 - 0x0048 - OLE Guid
					 258 - 0x0102 - Binary data

						 - 0x1003 - Array of 32bit values
						 - 0x101E - Array of Strings
						 - 0x1102 - Array of Binary data
						*/
					//	DEBUG_EMAIL(("Unknown id [%#x, size=%#x]\n", list->items[x]->id, list->items[x]->size));
					if (list->items[x]->type == 0x02) {
						DEBUG_EMAIL(("Unknown 16bit int = %hi\n", *(int16_t*)list->items[x]->data));
					} else if (list->items[x]->type == 0x03) {
						DEBUG_EMAIL(("Unknown 32bit int = %i\n", *(int32_t*)list->items[x]->data));
					} else if (list->items[x]->type == 0x0b) {
						DEBUG_EMAIL(("Unknown 16bit boolean = %s [%hi]\n",
								 (*((int16_t*)list->items[x]->data)!=0?"True":"False"),
								 *((int16_t*)list->items[x]->data)));
					} else if (list->items[x]->type == 0x1e) {
						DEBUG_EMAIL(("Unknown String Data = \"%s\" [%#x]\n",
								list->items[x]->data, list->items[x]->type));
					} else if (list->items[x]->type == 0x40) {
						DEBUG_EMAIL(("Unknown Date = \"%s\" [%#x]\n",
								fileTimeToAscii((FILETIME*)list->items[x]->data),
								list->items[x]->type));
					} else if (list->items[x]->type == 0x102) {
						DEBUG_EMAIL(("Unknown Binary Data [size = %#x]\n",
								 list->items[x]->size));
						DEBUG_HEXDUMP(list->items[x]->data, list->items[x]->size);
					} else if (list->items[x]->type == 0x101E) {
						DEBUG_EMAIL(("Unknown Array of Strings [%#x]\n",
								list->items[x]->type));
					} else {
						DEBUG_EMAIL(("Unknown Not Printable [%#x]\n",
								list->items[x]->type));
					}
					if (list->items[x]->data) {
						free(list->items[x]->data);
						list->items[x]->data = NULL;
					}
			}
			x++;
		}
		x = 0;
		list = list->next;
		next = 1;
	}
	DEBUG_RET();
	return 0;
}


int32_t _pst_free_list(pst_num_array *list) {
	pst_num_array *l;
	DEBUG_ENT("_pst_free_list");
	while (list) {
		if (list->items) {
			int32_t x;
			for (x=0; x < list->orig_count; x++) {
				if (list->items[x]) {
					if (list->items[x]->data) free(list->items[x]->data);
					free(list->items[x]);
				}
			}
			free(list->items);
		}
		l = list;
		list = list->next;
		free (l);
	}
	DEBUG_RET();
	return 1;
}


int32_t _pst_free_id2(pst_index2_ll * head) {
	pst_index2_ll *t;
	DEBUG_ENT("_pst_free_id2");
	while (head) {
		t = head->next;
		free (head);
		head = t;
	}
	DEBUG_RET();
	return 1;
}


int32_t _pst_free_id (pst_index_ll *head) {
	pst_index_ll *t;
	DEBUG_ENT("_pst_free_id");
	while (head) {
		t = head->next;
		free(head);
		head = t;
	}
	DEBUG_RET();
	return 1;
}


int32_t _pst_free_desc (pst_desc_ll *head) {
	pst_desc_ll *t;
	DEBUG_ENT("_pst_free_desc");
	while (head) {
		while (head->child) {
			head = head->child;
		}

		// point t to the next item
		t = head->next;
		if (!t && head->parent) {
			t = head->parent;
			t->child = NULL; // set the child to NULL so we don't come back here again!
		}

		if (head) free(head);
		else	  DIE(("head is NULL"));

		head = t;
	}
	DEBUG_RET();
	return 1;
}


int32_t _pst_free_xattrib(pst_x_attrib_ll *x) {
	pst_x_attrib_ll *t;
	DEBUG_ENT("_pst_free_xattrib");
	while (x) {
		if (x->data) free(x->data);
		t = x->next;
		free(x);
		x = t;
	}
	DEBUG_RET();
	return 1;
}


pst_index2_ll * _pst_build_id2(pst_file *pf, pst_index_ll* list, pst_index2_ll* head_ptr) {
	pst_block_header block_head;
	pst_index2_ll *head = NULL, *tail = NULL;
	int32_t x = 0, b_ptr = 0;
	char *buf = NULL;
	pst_id2_assoc id2_rec;
	pst_index_ll *i_ptr = NULL;
	pst_index2_ll *i2_ptr = NULL;
	DEBUG_ENT("_pst_build_id2");

	if (head_ptr) {
		head = head_ptr;
		while (head_ptr) head_ptr = (tail = head_ptr)->next;
	}
	if (_pst_read_block_size(pf, list->offset, list->size, &buf, PST_NO_ENC, 0) < list->size) {
		//an error occured in block read
		WARN(("block read error occured. offset = %#x, size = %#x\n", list->offset, list->size));
		if (buf) free(buf);
		DEBUG_RET();
		return NULL;
	}
	DEBUG_HEXDUMPC(buf, list->size, 16);

	memcpy(&block_head, buf, sizeof(block_head));
	LE16_CPU(block_head.type);
	LE16_CPU(block_head.count);

	if (block_head.type != 0x0002) { // some sort of constant?
		WARN(("Unknown constant [%#x] at start of id2 values [offset %#x].\n", block_head.type, list->offset));
		if (buf) free(buf);
		DEBUG_RET();
		return NULL;
	}

	DEBUG_INDEX(("ID %#x is likely to be a description record. Count is %i (offset %#x)\n",
			list->id, block_head.count, list->offset));
	x = 0;
	b_ptr = 0x04;
	while (x < block_head.count) {
		memcpy(&id2_rec, &(buf[b_ptr]), sizeof(id2_rec));
		LE32_CPU(id2_rec.id2);
		LE32_CPU(id2_rec.id);
		LE32_CPU(id2_rec.table2);

		b_ptr += sizeof(id2_rec);
		DEBUG_INDEX(("\tid2 = %#x, id = %#x, table2 = %#x\n", id2_rec.id2, id2_rec.id, id2_rec.table2));
		if ((i_ptr = _pst_getID(pf, id2_rec.id)) == NULL) {
			DEBUG_WARN(("\t\t%#x - Not Found\n", id2_rec.id));
		} else {
			DEBUG_INDEX(("\t\t%#x - Offset %#x, u1 %#x, Size %i(%#x)\n", i_ptr->id, i_ptr->offset, i_ptr->u1, i_ptr->size, i_ptr->size));
			// add it to the linked list
			i2_ptr = (pst_index2_ll*) xmalloc(sizeof(pst_index2_ll));
			i2_ptr->id2  = id2_rec.id2;
			i2_ptr->id	 = i_ptr;
			i2_ptr->next = NULL;
			if (!head) head = i2_ptr;
			if (tail)  tail->next = i2_ptr;
			tail = i2_ptr;
			if (id2_rec.table2 != 0) {
				if ((i_ptr = _pst_getID(pf, id2_rec.table2)) == NULL) {
					DEBUG_WARN(("\tTable2 [%#x] not found\n", id2_rec.table2));
				}
				else {
					DEBUG_INDEX(("\tGoing deeper for table2 [%#x]\n", id2_rec.table2));
					if ((i2_ptr = _pst_build_id2(pf, i_ptr, head))) {
					//	DEBUG_INDEX(("_pst_build_id2(): \t\tAdding new list onto end of current\n"));
					//	if (!head)
					//	  head = i2_ptr;
					//	if (tail)
					//	  tail->next = i2_ptr;
					//	while (i2_ptr->next)
					//	  i2_ptr = i2_ptr->next;
					//	  tail = i2_ptr;
					}
					// need to re-establish tail
					DEBUG_INDEX(("Returned from depth\n"));
					if (tail) {
						while (tail->next) tail = tail->next;
					}
				}
			}
		}
		x++;
	}
	if (buf) free (buf);
	DEBUG_RET();
	return head;
}


// This version of free does NULL check first
#define SAFE_FREE(x) {if (x) free(x);}

void _pst_freeItem(pst_item *item) {
	pst_item_attach *t;
	pst_item_extra_field *et;

	DEBUG_ENT("_pst_freeItem");
	if (item) {
		if (item->email) {
			SAFE_FREE(item->email->arrival_date);
			SAFE_FREE(item->email->body);
			SAFE_FREE(item->email->cc_address);
			SAFE_FREE(item->email->common_name);
			SAFE_FREE(item->email->encrypted_body);
			SAFE_FREE(item->email->encrypted_htmlbody);
			SAFE_FREE(item->email->header);
			SAFE_FREE(item->email->htmlbody);
			SAFE_FREE(item->email->in_reply_to);
			SAFE_FREE(item->email->messageid);
			SAFE_FREE(item->email->outlook_recipient);
			SAFE_FREE(item->email->outlook_recipient2);
			SAFE_FREE(item->email->outlook_sender);
			SAFE_FREE(item->email->outlook_sender_name);
			SAFE_FREE(item->email->outlook_sender2);
			SAFE_FREE(item->email->proc_subject);
			SAFE_FREE(item->email->recip_access);
			SAFE_FREE(item->email->recip_address);
			SAFE_FREE(item->email->recip2_access);
			SAFE_FREE(item->email->recip2_address);
			SAFE_FREE(item->email->reply_to);
			SAFE_FREE(item->email->rtf_body_tag);
			SAFE_FREE(item->email->rtf_compressed);
			SAFE_FREE(item->email->return_path_address);
			SAFE_FREE(item->email->sender_access);
			SAFE_FREE(item->email->sender_address);
			SAFE_FREE(item->email->sender2_access);
			SAFE_FREE(item->email->sender2_address);
			SAFE_FREE(item->email->sent_date);
			SAFE_FREE(item->email->sentmail_folder);
			SAFE_FREE(item->email->sentto_address);
			if (item->email->subject)
				SAFE_FREE(item->email->subject->subj);
			SAFE_FREE(item->email->subject);
			free(item->email);
		}
		if (item->folder) {
			free(item->folder);
		}
		if (item->message_store) {
			SAFE_FREE(item->message_store->deleted_items_folder);
			SAFE_FREE(item->message_store->search_root_folder);
			SAFE_FREE(item->message_store->top_of_personal_folder);
			SAFE_FREE(item->message_store->top_of_folder);
			free(item->message_store);
		}
		if (item->contact) {
			SAFE_FREE(item->contact->access_method);
			SAFE_FREE(item->contact->account_name);
			SAFE_FREE(item->contact->address1);
			SAFE_FREE(item->contact->address1a);
			SAFE_FREE(item->contact->address1_desc);
			SAFE_FREE(item->contact->address1_transport);
			SAFE_FREE(item->contact->address2);
			SAFE_FREE(item->contact->address2a);
			SAFE_FREE(item->contact->address2_desc);
			SAFE_FREE(item->contact->address2_transport);
			SAFE_FREE(item->contact->address3);
			SAFE_FREE(item->contact->address3a);
			SAFE_FREE(item->contact->address3_desc);
			SAFE_FREE(item->contact->address3_transport);
			SAFE_FREE(item->contact->assistant_name);
			SAFE_FREE(item->contact->assistant_phone);
			SAFE_FREE(item->contact->billing_information);
			SAFE_FREE(item->contact->birthday);
			SAFE_FREE(item->contact->business_address);
			SAFE_FREE(item->contact->business_city);
			SAFE_FREE(item->contact->business_country);
			SAFE_FREE(item->contact->business_fax);
			SAFE_FREE(item->contact->business_homepage);
			SAFE_FREE(item->contact->business_phone);
			SAFE_FREE(item->contact->business_phone2);
			SAFE_FREE(item->contact->business_po_box);
			SAFE_FREE(item->contact->business_postal_code);
			SAFE_FREE(item->contact->business_state);
			SAFE_FREE(item->contact->business_street);
			SAFE_FREE(item->contact->callback_phone);
			SAFE_FREE(item->contact->car_phone);
			SAFE_FREE(item->contact->company_main_phone);
			SAFE_FREE(item->contact->company_name);
			SAFE_FREE(item->contact->computer_name);
			SAFE_FREE(item->contact->customer_id);
			SAFE_FREE(item->contact->def_postal_address);
			SAFE_FREE(item->contact->department);
			SAFE_FREE(item->contact->display_name_prefix);
			SAFE_FREE(item->contact->first_name);
			SAFE_FREE(item->contact->followup);
			SAFE_FREE(item->contact->free_busy_address);
			SAFE_FREE(item->contact->ftp_site);
			SAFE_FREE(item->contact->fullname);
			SAFE_FREE(item->contact->gov_id);
			SAFE_FREE(item->contact->hobbies);
			SAFE_FREE(item->contact->home_address);
			SAFE_FREE(item->contact->home_city);
			SAFE_FREE(item->contact->home_country);
			SAFE_FREE(item->contact->home_fax);
			SAFE_FREE(item->contact->home_po_box);
			SAFE_FREE(item->contact->home_phone);
			SAFE_FREE(item->contact->home_phone2);
			SAFE_FREE(item->contact->home_postal_code);
			SAFE_FREE(item->contact->home_state);
			SAFE_FREE(item->contact->home_street);
			SAFE_FREE(item->contact->initials);
			SAFE_FREE(item->contact->isdn_phone);
			SAFE_FREE(item->contact->job_title);
			SAFE_FREE(item->contact->keyword);
			SAFE_FREE(item->contact->language);
			SAFE_FREE(item->contact->location);
			SAFE_FREE(item->contact->manager_name);
			SAFE_FREE(item->contact->middle_name);
			SAFE_FREE(item->contact->mileage);
			SAFE_FREE(item->contact->mobile_phone);
			SAFE_FREE(item->contact->nickname);
			SAFE_FREE(item->contact->office_loc);
			SAFE_FREE(item->contact->org_id);
			SAFE_FREE(item->contact->other_address);
			SAFE_FREE(item->contact->other_city);
			SAFE_FREE(item->contact->other_country);
			SAFE_FREE(item->contact->other_phone);
			SAFE_FREE(item->contact->other_po_box);
			SAFE_FREE(item->contact->other_postal_code);
			SAFE_FREE(item->contact->other_state);
			SAFE_FREE(item->contact->other_street);
			SAFE_FREE(item->contact->pager_phone);
			SAFE_FREE(item->contact->personal_homepage);
			SAFE_FREE(item->contact->pref_name);
			SAFE_FREE(item->contact->primary_fax);
			SAFE_FREE(item->contact->primary_phone);
			SAFE_FREE(item->contact->profession);
			SAFE_FREE(item->contact->radio_phone);
			SAFE_FREE(item->contact->spouse_name);
			SAFE_FREE(item->contact->suffix);
			SAFE_FREE(item->contact->surname);
			SAFE_FREE(item->contact->telex);
			SAFE_FREE(item->contact->transmittable_display_name);
			SAFE_FREE(item->contact->ttytdd_phone);
			SAFE_FREE(item->contact->wedding_anniversary);
			free(item->contact);
		}
		while (item->attach) {
			SAFE_FREE(item->attach->filename1);
			SAFE_FREE(item->attach->filename2);
			SAFE_FREE(item->attach->mimetype);
			SAFE_FREE(item->attach->data);
			t = item->attach->next;
			free(item->attach);
			item->attach = t;
		}
		while (item->extra_fields) {
			SAFE_FREE(item->extra_fields->field_name);
			SAFE_FREE(item->extra_fields->value);
			et = item->extra_fields->next;
			free(item->extra_fields);
			item->extra_fields = et;
		}
		if (item->journal) {
			SAFE_FREE(item->journal->end);
			SAFE_FREE(item->journal->start);
			SAFE_FREE(item->journal->type);
			free(item->journal);
		}
		if (item->appointment) {
			SAFE_FREE(item->appointment->location);
			SAFE_FREE(item->appointment->reminder);
			SAFE_FREE(item->appointment->start);
			SAFE_FREE(item->appointment->end);
			SAFE_FREE(item->appointment->timezonestring);
			free(item->appointment);
		}
		SAFE_FREE(item->ascii_type);
		SAFE_FREE(item->comment);
		SAFE_FREE(item->create_date);
		SAFE_FREE(item->file_as);
		SAFE_FREE(item->modify_date);
		SAFE_FREE(item->outlook_version);
		SAFE_FREE(item->record_key);
		free(item);
	}
	DEBUG_RET();
}


/**
  * The offset might be zero, in which case we have no data, so return a pair of null pointers.
  * Or, the offset might end in 0xf, so it is an id2 pointer, in which case we read the id2 block.
  * Otherwise, the offset>>4 is an index into the table of offsets in the buffer.
*/
int32_t _pst_getBlockOffsetPointer(pst_file *pf, pst_index2_ll *i2_head, unsigned char *buf, int32_t read_size, int32_t i_offset, int32_t offset, pst_block_offset_pointer *p) {
	int32_t size;
	pst_block_offset block_offset;
	DEBUG_ENT("_pst_getBlockOffsetPointer");
	if (p->needfree) free(p->from);
	p->from = NULL;
	p->needfree = 0;
	if (!offset) {
		p->from = p->to = NULL;
	}
	else if ((offset & 0xf) == 0xf) {
		DEBUG_WARN(("Found id2 %#x value. Will follow it\n", offset));
		size = _pst_ff_getID2block(pf, offset, i2_head, &(p->from));
		if (size) {
			p->to = p->from + size;
			p->needfree = 1;
		}
		else {
			p->from = p->to = NULL;
		}
	}
	else if (_pst_getBlockOffset(buf, read_size, i_offset, offset, &block_offset)) {
		p->from = p->to = NULL;
	}
	else {
		p->from = buf + block_offset.from;
		p->to	= buf + block_offset.to;
	}
	DEBUG_RET();
	return (p->from) ? 0 : 1;
}


int32_t _pst_getBlockOffset(unsigned char *buf, int32_t read_size, int32_t i_offset, int32_t offset, pst_block_offset *p) {
	int32_t low = offset & 0xf;
	int32_t of1 = offset >> 4;
	DEBUG_ENT("_pst_getBlockOffset");
	if (!p || !buf || !i_offset || low || (i_offset+2+of1+sizeof(*p) > read_size)) {
		DEBUG_WARN(("p is NULL or buf is NULL or offset is 0 or offset has low bits or beyond read size (%p, %p, %#x, %i, %i)\n", p, buf, offset, read_size, i_offset));
		DEBUG_RET();
		return -1;
	}
	memcpy(&(p->from), &(buf[(i_offset+2)+of1]), sizeof(p->from));
	memcpy(&(p->to), &(buf[(i_offset+2)+of1+sizeof(p->from)]), sizeof(p->to));
	LE16_CPU(p->from);
	LE16_CPU(p->to);
	DEBUG_WARN(("get block offset finds from=%i(%#x), to=%i(%#x)\n", p->from, p->from, p->to, p->to));
	if (p->from > p->to) {
		DEBUG_WARN(("get block offset from > to"));
		return -1;
	}
	DEBUG_RET();
	return 0;
}


pst_index_ll* _pst_getID(pst_file* pf, u_int32_t id) {
	pst_index_ll *ptr = NULL;
	DEBUG_ENT("_pst_getID");
	if (id == 0) {
		DEBUG_RET();
		return NULL;
	}

	//if (id & 1) DEBUG_INDEX(("have odd id bit %#x\n", id));
	//if (id & 2) DEBUG_INDEX(("have two id bit %#x\n", id));
	id &= 0xFFFFFFFE;

	DEBUG_INDEX(("Trying to find %#x\n", id));
	if (!ptr) ptr = pf->i_head;
	while (ptr && (ptr->id != id)) {
		ptr = ptr->next;
	}
	if (ptr) {DEBUG_INDEX(("Found Value %#x\n", id));            }
	else	 {DEBUG_INDEX(("ERROR: Value %#x not found\n", id)); }
	DEBUG_RET();
	return ptr;
}


pst_index_ll * _pst_getID2(pst_index2_ll *ptr, u_int32_t id) {
	DEBUG_ENT("_pst_getID2");
	DEBUG_INDEX(("Head = %p\n", ptr));
	DEBUG_INDEX(("Trying to find %#x\n", id));
	while (ptr && (ptr->id2 != id)) {
		ptr = ptr->next;
	}
	if (ptr) {
		if (ptr->id) {DEBUG_INDEX(("Found value %#x\n", ptr->id->id));   }
		else		 {DEBUG_INDEX(("Found value, though it is NULL!\n"));}
		DEBUG_RET();
		return ptr->id;
	}
	DEBUG_INDEX(("ERROR Not Found\n"));
	DEBUG_RET();
	return NULL;
}


/**
 * find the id in the descriptor tree rooted at pf->d_head
 *
 * @param pf	global pst file pointer
 * @param id	the id we are looking for
 *
 * @return pointer to the pst_desc_ll node in the descriptor tree
*/
pst_desc_ll* _pst_getDptr(pst_file *pf, u_int32_t id) {
	pst_desc_ll *ptr = pf->d_head;
	DEBUG_ENT("_pst_getDptr");
	while (ptr && (ptr->id != id)) {
		if (ptr->child) {
			ptr = ptr->child;
			continue;
		}
		while (!ptr->next && ptr->parent) {
			ptr = ptr->parent;
		}
		ptr = ptr->next;
	}
	DEBUG_RET();
	return ptr; // will be NULL or record we are looking for
}


int32_t _pst_printDptr(pst_file *pf) {
	pst_desc_ll *ptr = pf->d_head;
	int32_t depth = 0;
	char spaces[100];
	DEBUG_ENT("_pst_printDptr");
	memset(spaces, ' ', 99);
	spaces[99] = '\0';
	while (ptr) {
		DEBUG_INDEX(("%s%#x [%i] desc=%#x, list=%#x\n", &(spaces[(99-depth<0?0:99-depth)]), ptr->id, ptr->no_child,
			(ptr->desc==NULL?0:ptr->desc->id),
			(ptr->list_index==NULL?0:ptr->list_index->id)));
		if (ptr->child) {
			depth++;
			ptr = ptr->child;
			continue;
		}
		while (!ptr->next && ptr->parent) {
			depth--;
			ptr = ptr->parent;
		}
		ptr = ptr->next;
	}
	DEBUG_RET();
	return 0;
}


int32_t _pst_printIDptr(pst_file* pf) {
	pst_index_ll *ptr = pf->i_head;
	DEBUG_ENT("_pst_printIDptr");
	while (ptr) {
		DEBUG_INDEX(("%#x offset=%#x size=%#x\n", ptr->id, ptr->offset, ptr->size));
		ptr = ptr->next;
	}
	DEBUG_RET();
	return 0;
}


int32_t _pst_printID2ptr(pst_index2_ll *ptr) {
	DEBUG_ENT("_pst_printID2ptr");
	while (ptr) {
		DEBUG_INDEX(("%#x id=%#x\n", ptr->id2, (ptr->id!=NULL?ptr->id->id:0)));
		ptr = ptr->next;
	}
	DEBUG_RET();
	return 0;
}


size_t _pst_read_block(FILE *fp, int32_t offset, void **buf) {
	size_t size;
	int32_t fpos;
	DEBUG_ENT("_pst_read_block");
	DEBUG_READ(("Reading block from %#x\n", offset));
	fpos = ftell(fp);
	fseek(fp, offset, SEEK_SET);
	fread(&size, sizeof(int16_t), 1, fp);
	fseek(fp, offset, SEEK_SET);
	DEBUG_READ(("Allocating %i bytes\n", size));
	if (*buf) {
		DEBUG_READ(("Freeing old memory\n"));
		free(*buf);
	}
	*buf = (void*)xmalloc(size);
	size = fread(*buf, 1, size, fp);
	fseek(fp, fpos, SEEK_SET);
	DEBUG_RET();
	return size;
}


// when the first byte of the block being read is 01, then we can assume
// that it is a list of further ids to read and we will follow those ids
// recursively calling this function until we have all the data
// we could do decryption of the encrypted PST files here
size_t _pst_read_block_size(pst_file *pf, int32_t offset, size_t size, char ** buf, int32_t do_enc, unsigned char is_index) {
	u_int32_t fpos, x;
	int16_t count, y;
	char *buf2 = NULL, *buf3 = NULL;
	unsigned char fdepth;
	pst_index_ll *ptr = NULL;
	size_t rsize, z;

	DEBUG_ENT("_pst_read_block_size");
	DEBUG_READ(("Reading block from %#x, %i bytes\n", offset, size));

	fpos = ftell(pf->fp);
	fseek(pf->fp, offset, SEEK_SET);
	if (*buf) {
		DEBUG_READ(("Freeing old memory\n"));
		free(*buf);
	}

	*buf = (void*) xmalloc(size+1); //plus one so that we can NUL terminate it later
	rsize = fread(*buf, 1, size, pf->fp);
	if (rsize != size) {
		DEBUG_WARN(("Didn't read all that I could. fread returned less [%i instead of %i]\n", rsize, size));
		if (feof(pf->fp)) {
			DEBUG_WARN(("We tried to read past the end of the file at [offset %#x, size %#x]\n", offset, size));
		} else if (ferror(pf->fp)) {
			DEBUG_WARN(("Error is set on file stream.\n"));
		} else {
			DEBUG_WARN(("I can't tell why it failed\n"));
		}
		size = rsize;
	}

	//	DEBUG_HEXDUMP(*buf, size);

	/*	if (is_index) {
	  DEBUG_READ(("_pst_read_block_size: ODD_BLOCK should be here\n"));
	  DEBUG_READ(("\t: byte 0-1: %#x %#x\n", (*buf)[0], (*buf)[1]));
	  }*/

	if ((*buf)[0] == 0x01 && (*buf)[1] != 0x00 && is_index) {
		//don't do this recursion if we should be at a leaf node
		memcpy(&count, &((*buf)[2]), sizeof(int16_t));
		LE16_CPU(count);
		memcpy(&fdepth, &((*buf)[1]), sizeof(fdepth));
		DEBUG_READ(("Seen indexes to blocks. Depth is %i\n", fdepth));
		// do fancy stuff! :)
		DEBUG_READ(("There are %i ids\n", count));
		// if first 2 blocks are 01 01 then index to blocks
		size = 0;
		y = 0;
		while (y < count) {
			memcpy(&x, &(*buf)[0x08+(y*4)], sizeof(int32_t));
			LE32_CPU(x);
			if ((ptr = _pst_getID(pf, x)) == NULL) {
				WARN(("Error. Cannot find ID [%#x] during multi-block read\n", x));
				buf3 = (char*) realloc(buf3, size+1);
				buf3[size] = '\0';
				*buf = buf3;
				fseek(pf->fp, fpos, SEEK_SET);
				DEBUG_RET();
				return size;
			}
			if ((z = _pst_read_block_size(pf, ptr->offset, ptr->size, &buf2, do_enc, fdepth-1)) < ptr->size) {
				buf3 = (char*) realloc(buf3, size+1);
				buf3[size] = '\0';
				*buf = buf3;
				fseek(pf->fp, fpos, SEEK_SET);
				DEBUG_RET();
				return size;
			}
			DEBUG_READ(("Melding newley retrieved block with bigger one. New size is %i\n", size+z));
			buf3 = (char*) realloc(buf3, size+z+1); //plus one so that we can null terminate it later
			DEBUG_READ(("Doing copy. Start pos is %i, length is %i\n", size, z));
			memcpy(&(buf3[size]), buf2, z);
			size += z;
			y++;
		}
		free(*buf);
		if (buf2) free(buf2);
		if (!buf3) {
			// this can happen if count == 0. We should create an empty buffer so we don't
			// confuse any clients
			buf3 = (char*) xmalloc(1);
		}
		*buf = buf3;
	} else if (do_enc && pf->encryption)
		_pst_decrypt(*buf, size, pf->encryption);

	(*buf)[size] = '\0'; //should be byte after last one read
	fseek(pf->fp, fpos, SEEK_SET);
	DEBUG_RET();
	return size;
}


int32_t _pst_decrypt(unsigned char *buf, size_t size, int32_t type) {
	size_t x = 0;
	unsigned char y;
	DEBUG_ENT("_pst_decrypt");
	if (!buf) {
		DEBUG_RET();
		return -1;
	}

	if (type == PST_COMP_ENCRYPT) {
		x = 0;
		while (x < size) {
			y = buf[x];
			DEBUG_DECRYPT(("Transposing %#hhx to %#hhx [%#x]\n", buf[x], comp_enc[y], y));
			buf[x] = comp_enc[y]; // transpose from encrypt array
			x++;
		}
	} else {
		WARN(("Unknown encryption: %i. Cannot decrypt\n", type));
		DEBUG_RET();
		return -1;
	}
	DEBUG_RET();
	return 0;
}


int32_t _pst_getAtPos(FILE *fp, int32_t pos, void* buf, u_int32_t size) {
	DEBUG_ENT("_pst_getAtPos");
	if (fseek(fp, pos, SEEK_SET) == -1) {
		DEBUG_RET();
		return 1;
	}

	if (fread(buf, 1, size, fp) < size) {
		DEBUG_RET();
		return 2;
	}
	DEBUG_RET();
	return 0;
}


int32_t _pst_get (FILE *fp, void *buf, u_int32_t size) {
	DEBUG_ENT("_pst_get");
	if (fread(buf, 1,  size, fp) < size) {
		DEBUG_RET();
		return 1;
	}
	DEBUG_RET();
	return 0;
}


size_t _pst_ff_getIDblock_dec(pst_file *pf, u_int32_t id, unsigned char **b) {
	size_t r;
	DEBUG_ENT("_pst_ff_getIDblock_dec");
	DEBUG_INDEX(("for id %#x\n", id));
	r = _pst_ff_getIDblock(pf, id, b);
	int noenc = (id & 2);	// disable encryption
	if ((pf->encryption) & !(noenc)) {
		_pst_decrypt(*b, r, pf->encryption);
	}
	DEBUG_HEXDUMPC(*b, r, 16);
	DEBUG_RET();
	return r;
}


size_t _pst_ff_getIDblock(pst_file *pf, u_int32_t id, unsigned char** b) {
	pst_index_ll *rec;
	size_t rsize = 0;//, re_size=0;
	DEBUG_ENT("_pst_ff_getIDblock");
	if ((rec = _pst_getID(pf, id)) == NULL) {
		DEBUG_INDEX(("Cannot find ID %#x\n", id));
		DEBUG_RET();
		return 0;
	}
	fseek(pf->fp, rec->offset, SEEK_SET);
	if (*b) {
		DEBUG_INDEX(("freeing old memory in b\n"));
		free(*b);
	}

	DEBUG_INDEX(("id = %#x, record size = %#x, offset = %#x\n", id, rec->size, rec->offset));
	*b = (char*) xmalloc(rec->size+1);
	rsize = fread(*b, 1, rec->size, pf->fp);
	if (rsize != rec->size) {
		DEBUG_WARN(("Didn't read all the size. fread returned less [%i instead of %i]\n", rsize, rec->size));
		if (feof(pf->fp)) {
			DEBUG_WARN(("We tried to read past the end of the file [offset %#x, size %#x]\n", rec->offset, rec->size));
		} else if (ferror(pf->fp)) {
			DEBUG_WARN(("Some error occured on the file stream\n"));
		} else {
			DEBUG_WARN(("No error has been set on the file stream\n"));
		}
	}
	DEBUG_RET();
	return rsize;
}


#define PST_PTR_BLOCK_SIZE 0x120
size_t _pst_ff_getID2block(pst_file *pf, u_int32_t id2, pst_index2_ll *id2_head, unsigned char** buf) {
	pst_index_ll* ptr;
	//	size_t ret;
	struct holder h = {buf, NULL, 0, "", 0};
	DEBUG_ENT("_pst_ff_getID2block");
	ptr = _pst_getID2(id2_head, id2);

	if (!ptr) {
		DEBUG_INDEX(("Cannot find id2 value %#x\n", id2));
		DEBUG_RET();
		return 0;
	}
	DEBUG_RET();
	return _pst_ff_getID2data(pf, ptr, &h);
}


size_t _pst_ff_getID2data(pst_file *pf, pst_index_ll *ptr, struct holder *h) {
	int32_t ret;
	unsigned char *b = NULL, *t;
	DEBUG_ENT("_pst_ff_getID2data");
	if (!(ptr->id & 0x02)) {
		ret = _pst_ff_getIDblock_dec(pf, ptr->id, &b);
		if (h->buf) {
			*(h->buf) = b;
		} else if ((h->base64 == 1) && h->fp) {
			t = base64_encode(b, ret);
			if (t) {
				pst_fwrite(t, 1, strlen(t), h->fp);
				free(t);	// caught by valgrind
			}
			free(b);
		} else if (h->fp) {
			pst_fwrite(b, 1, ret, h->fp);
			free(b);
		}
	} else {
		// here we will assume it is a block that points to others
		DEBUG_READ(("Assuming it is a multi-block record because of it's id\n"));
		ret = _pst_ff_compile_ID(pf, ptr->id, h, 0);
	}
	if (h->buf && *h->buf)
		(*(h->buf))[ret]='\0';
	DEBUG_RET();
	return ret;
}


size_t _pst_ff_compile_ID(pst_file *pf, u_int32_t id, struct holder *h, int32_t size) {
	size_t z, a;
	u_int16_t count, y;
	u_int32_t x, b;
	unsigned char * buf3 = NULL, *buf2 = NULL, *t;
	unsigned char fdepth;

	DEBUG_ENT("_pst_ff_compile_ID");
	a = _pst_ff_getIDblock(pf, id, &buf3);
	if (!a) {
		if (buf3) free(buf3);
		return 0;
	}
	if ((buf3[0] != 0x1)) { // if bit 8 is set) {
		//	if ((buf3)[0] != 0x1 && (buf3)[1] > 4) {
		DEBUG_WARN(("WARNING: buffer doesn't start with 0x1, but I expected it to or doesn't have it's two-bit set!\n"));
		DEBUG_WARN(("Treating as normal buffer\n"));
		if (pf->encryption) _pst_decrypt(buf3, a, pf->encryption);
		if (h->buf)
			*(h->buf) = buf3;
		else if (h->base64 == 1 && h->fp) {
			t = base64_encode(buf3, a);
			if (t) {
				pst_fwrite(t, 1, strlen(t), h->fp);
				free(t);	// caught by valgrind
			}
			free(buf3);
		} else if (h->fp) {
			pst_fwrite(buf3, 1, a, h->fp);
			free(buf3);
		}
		DEBUG_RET();
		return a;
	}
	memcpy (&count, &(buf3[2]), sizeof(int16_t));
	LE16_CPU(count);
	memcpy (&fdepth, &(buf3[1]), sizeof(char));
	DEBUG_READ(("Seen index to blocks. Depth is %i\n", fdepth));
	DEBUG_READ(("There are %i ids here\n", count));

	y = 0;
	while (y < count) {
		memcpy(&x, &buf3[0x08+(y*4)], sizeof(int32_t));
		LE32_CPU(x);
		if (fdepth == 0x1) {
			if ((z = _pst_ff_getIDblock(pf, x, &buf2)) == 0) {
				DEBUG_WARN(("call to getIDblock returned zero %i\n", z));
				if (buf2) free(buf2);
				free(buf3);
				return z;
			}
			if (pf->encryption) _pst_decrypt(buf2, z, pf->encryption);
			if (h->buf) {
				*(h->buf) = realloc(*(h->buf), size+z+1);
				DEBUG_READ(("appending read data of size %i onto main buffer from pos %i\n", z, size));
				memcpy(&((*(h->buf))[size]), buf2, z);
			}
			else if ((h->base64 == 1) && h->fp) {
				// include any byte left over from the last one encoding
				buf2 = (char*)realloc(buf2, z+h->base64_extra);
				memmove(buf2+h->base64_extra, buf2, z);
				memcpy(buf2, h->base64_extra_chars, h->base64_extra);
				z+= h->base64_extra;

				b = z % 3; // find out how many bytes will be left over after the encoding.
				// and save them
				memcpy(h->base64_extra_chars, &(buf2[z-b]), b);
				h->base64_extra = b;
				t = base64_encode(buf2, z-b);
				if (t) {
					DEBUG_READ(("writing %i bytes to file as base64 [%i]. Currently %i\n", z, strlen(t), size));
					pst_fwrite(t, 1, strlen(t), h->fp);
					free(t);	// caught by valgrind
				}
			}
			else if (h->fp) {
				DEBUG_READ(("writing %i bytes to file. Currently %i\n", z, size));
				pst_fwrite(buf2, 1, z, h->fp);
			}
			size += z;
			y++;
		}
		else {
			if ((z = _pst_ff_compile_ID(pf, x, h, size)) == 0) {
				DEBUG_WARN(("recursive called returned zero %i\n", z));
				free(buf3);
				DEBUG_RET();
				return z;
			}
			size = z;
			y++;
		}
	}
	free(buf3);
	if (buf2) free(buf2);
	DEBUG_RET();
	return size;
}


#ifdef _MSC_VER
char * fileTimeToAscii(const FILETIME* filetime) {
	time_t t;
	DEBUG_ENT("fileTimeToAscii");
	t = fileTimeToUnixTime(filetime, 0);
	if (t == -1)
		DEBUG_WARN(("ERROR time_t varible that was produced, is -1\n"));
	DEBUG_RET();
	return ctime(&t);
}


time_t fileTimeToUnixTime(const FILETIME* filetime, DWORD *x) {
	SYSTEMTIME s;
	struct tm t;
	DEBUG_ENT("fileTimeToUnixTime");
	memset (&t, 0, sizeof(struct tm));
	FileTimeToSystemTime(filetime, &s);
	t.tm_year = s.wYear-1900; // this is what is required
	t.tm_mon = s.wMonth-1; // also required! It made me a bit confused
	t.tm_mday = s.wDay;
	t.tm_hour = s.wHour;
	t.tm_min = s.wMinute;
	t.tm_sec = s.wSecond;
	DEBUG_RET();
	return mktime(&t);
}


struct tm * fileTimeToStructTM (const FILETIME *filetime) {
	time_t t1;
	t1 = fileTimeToUnixTime(filetime, 0);
	return gmtime(&t1);
}


#endif //_MSC_VER

int32_t pst_stricmp(char *a, char *b) {
	// compare strings case-insensitive.
	// returns -1 if a < b, 0 if a==b, 1 if a > b
	while(*a != '\0' && *b != '\0' && toupper(*a)==toupper(*b)) {
		a++; b++;
	}
	if (toupper(*a) == toupper(*b))
		return 0;
	else if (toupper(*a) < toupper(*b))
		return -1;
	else
		return 1;
}


int32_t pst_strincmp(char *a, char *b, int32_t x) {
	// compare upto x chars in string a and b case-insensitively
	// returns -1 if a < b, 0 if a==b, 1 if a > b
	int32_t y = 0;
	while (*a != '\0' && *b != '\0' && y < x && toupper(*a)==toupper(*b)) {
		a++; b++; y++;
	}
	// if we have reached the end of either string, or a and b still match
	if (*a == '\0' || *b == '\0' || toupper(*a)==toupper(*b))
		return 0;
	else if (toupper(*a) < toupper(*b))
		return -1;
	else
		return 1;
}


size_t pst_fwrite(const void*ptr, size_t size, size_t nmemb, FILE*stream) {
	size_t r;
	DEBUG_ENT("pst_fwrite");
	if (ptr)
		r = fwrite(ptr, size, nmemb, stream);
	else {
		r = 0;
		DEBUG_WARN(("An attempt to write a NULL Pointer was made\n"));
	}
	DEBUG_RET();
	return r;
}


char * _pst_wide_to_single(char *wt, int32_t size) {
	// returns the first byte of each wide char. the size is the number of bytes in source
	char *x, *y;
	DEBUG_ENT("_pst_wide_to_single");
	x = xmalloc((size/2)+1);
	y = x;
	while (size != 0 && *wt != '\0') {
		*y = *wt;
		wt+=2;
		size -= 2;
		y++;
	}
	*y = '\0';
	DEBUG_RET();
	return x;
}

