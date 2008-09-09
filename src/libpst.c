/***
 * libpst.c
 * Part of the LibPST project
 * Written by David Smith
 *            dave.s@earthcorp.com
 */
#include "define.h"
#include "libstrfunc.h"
#include "vbuf.h"
#include "libpst.h"
#include "timeconv.h"

#define ASSERT(x) { if(!(x)) raise( SIGSEGV ); }


#define INDEX_TYPE32            0x0E
#define INDEX_TYPE64            0x17
#define INDEX_TYPE_OFFSET       (off_t)0x0A

#define FILE_SIZE_POINTER32     (off_t)0xA8
#define INDEX_POINTER32         (off_t)0xC4
#define INDEX_BACK32            (off_t)0xC0
#define SECOND_POINTER32        (off_t)0xBC
#define SECOND_BACK32           (off_t)0xB8
#define ENC_TYPE32              (off_t)0x1CD

#define FILE_SIZE_POINTER64     (off_t)0xB8
#define INDEX_POINTER64         (off_t)0xF0
#define INDEX_BACK64            (off_t)0xE8
#define SECOND_POINTER64        (off_t)0xE0
#define SECOND_BACK64           (off_t)0xD8
#define ENC_TYPE64              (off_t)0x201

#define FILE_SIZE_POINTER ((pf->do_read64) ? FILE_SIZE_POINTER64 : FILE_SIZE_POINTER32)
#define INDEX_POINTER     ((pf->do_read64) ? INDEX_POINTER64     : INDEX_POINTER32)
#define INDEX_BACK        ((pf->do_read64) ? INDEX_BACK64        : INDEX_BACK32)
#define SECOND_POINTER    ((pf->do_read64) ? SECOND_POINTER64    : SECOND_POINTER32)
#define SECOND_BACK       ((pf->do_read64) ? SECOND_BACK64       : SECOND_BACK32)
#define ENC_TYPE          ((pf->do_read64) ? ENC_TYPE64          : ENC_TYPE32)

#define PST_SIGNATURE 0x4E444221


struct pst_table_ptr_struct32{
  uint32_t start;
  uint32_t u1;
  uint32_t offset;
};


struct pst_table_ptr_structn{
  uint64_t start;
  uint64_t u1;
  uint64_t offset;
};


typedef struct pst_block_header {
    uint16_t type;
    uint16_t count;
} pst_block_header;


typedef struct pst_id2_assoc32 {
    uint32_t id2;
    uint32_t id;
    uint32_t table2;
} pst_id2_assoc32;


typedef struct pst_id2_assoc {
    uint32_t id2;       // only 32 bit here?
    uint16_t unknown1;
    uint16_t unknown2;
    uint64_t id;
    uint64_t table2;
} pst_id2_assoc;


typedef struct pst_table3_rec32 {
    uint32_t id;
} pst_table3_rec32; //for type 3 (0x0101) blocks


typedef struct pst_table3_rec {
    uint64_t id;
} pst_table3_rec;   //for type 3 (0x0101) blocks


typedef struct pst_block_hdr {
    uint16_t index_offset;
    uint16_t type;
    uint32_t offset;
} pst_block_hdr;


// for "compressible" encryption, just a simple substitution cipher
// this is an array of the un-encrypted values. the un-encrypted value is in the position
// of the encrypted value. ie the encrypted value 0x13 represents 0x02
static unsigned char comp_enc [] = {
    0x47, 0xf1, 0xb4, 0xe6, 0x0b, 0x6a, 0x72, 0x48, 0x85, 0x4e, 0x9e, 0xeb, 0xe2, 0xf8, 0x94, 0x53,
    0xe0, 0xbb, 0xa0, 0x02, 0xe8, 0x5a, 0x09, 0xab, 0xdb, 0xe3, 0xba, 0xc6, 0x7c, 0xc3, 0x10, 0xdd,
    0x39, 0x05, 0x96, 0x30, 0xf5, 0x37, 0x60, 0x82, 0x8c, 0xc9, 0x13, 0x4a, 0x6b, 0x1d, 0xf3, 0xfb,
    0x8f, 0x26, 0x97, 0xca, 0x91, 0x17, 0x01, 0xc4, 0x32, 0x2d, 0x6e, 0x31, 0x95, 0xff, 0xd9, 0x23,
    0xd1, 0x00, 0x5e, 0x79, 0xdc, 0x44, 0x3b, 0x1a, 0x28, 0xc5, 0x61, 0x57, 0x20, 0x90, 0x3d, 0x83,
    0xb9, 0x43, 0xbe, 0x67, 0xd2, 0x46, 0x42, 0x76, 0xc0, 0x6d, 0x5b, 0x7e, 0xb2, 0x0f, 0x16, 0x29,
    0x3c, 0xa9, 0x03, 0x54, 0x0d, 0xda, 0x5d, 0xdf, 0xf6, 0xb7, 0xc7, 0x62, 0xcd, 0x8d, 0x06, 0xd3,
    0x69, 0x5c, 0x86, 0xd6, 0x14, 0xf7, 0xa5, 0x66, 0x75, 0xac, 0xb1, 0xe9, 0x45, 0x21, 0x70, 0x0c,
    0x87, 0x9f, 0x74, 0xa4, 0x22, 0x4c, 0x6f, 0xbf, 0x1f, 0x56, 0xaa, 0x2e, 0xb3, 0x78, 0x33, 0x50,
    0xb0, 0xa3, 0x92, 0xbc, 0xcf, 0x19, 0x1c, 0xa7, 0x63, 0xcb, 0x1e, 0x4d, 0x3e, 0x4b, 0x1b, 0x9b,
    0x4f, 0xe7, 0xf0, 0xee, 0xad, 0x3a, 0xb5, 0x59, 0x04, 0xea, 0x40, 0x55, 0x25, 0x51, 0xe5, 0x7a,
    0x89, 0x38, 0x68, 0x52, 0x7b, 0xfc, 0x27, 0xae, 0xd7, 0xbd, 0xfa, 0x07, 0xf4, 0xcc, 0x8e, 0x5f,
    0xef, 0x35, 0x9c, 0x84, 0x2b, 0x15, 0xd5, 0x77, 0x34, 0x49, 0xb6, 0x12, 0x0a, 0x7f, 0x71, 0x88,
    0xfd, 0x9d, 0x18, 0x41, 0x7d, 0x93, 0xd8, 0x58, 0x2c, 0xce, 0xfe, 0x24, 0xaf, 0xde, 0xb8, 0x36,
    0xc8, 0xa1, 0x80, 0xa6, 0x99, 0x98, 0xa8, 0x2f, 0x0e, 0x81, 0x65, 0x73, 0xe4, 0xc2, 0xa2, 0x8a,
    0xd4, 0xe1, 0x11, 0xd0, 0x08, 0x8b, 0x2a, 0xf2, 0xed, 0x9a, 0x64, 0x3f, 0xc1, 0x6c, 0xf9, 0xec
};

// for "strong" encryption, we have the two additional tables
static unsigned char comp_high1 [] = {
    0x41, 0x36, 0x13, 0x62, 0xa8, 0x21, 0x6e, 0xbb, 0xf4, 0x16, 0xcc, 0x04, 0x7f, 0x64, 0xe8, 0x5d,
    0x1e, 0xf2, 0xcb, 0x2a, 0x74, 0xc5, 0x5e, 0x35, 0xd2, 0x95, 0x47, 0x9e, 0x96, 0x2d, 0x9a, 0x88,
    0x4c, 0x7d, 0x84, 0x3f, 0xdb, 0xac, 0x31, 0xb6, 0x48, 0x5f, 0xf6, 0xc4, 0xd8, 0x39, 0x8b, 0xe7,
    0x23, 0x3b, 0x38, 0x8e, 0xc8, 0xc1, 0xdf, 0x25, 0xb1, 0x20, 0xa5, 0x46, 0x60, 0x4e, 0x9c, 0xfb,
    0xaa, 0xd3, 0x56, 0x51, 0x45, 0x7c, 0x55, 0x00, 0x07, 0xc9, 0x2b, 0x9d, 0x85, 0x9b, 0x09, 0xa0,
    0x8f, 0xad, 0xb3, 0x0f, 0x63, 0xab, 0x89, 0x4b, 0xd7, 0xa7, 0x15, 0x5a, 0x71, 0x66, 0x42, 0xbf,
    0x26, 0x4a, 0x6b, 0x98, 0xfa, 0xea, 0x77, 0x53, 0xb2, 0x70, 0x05, 0x2c, 0xfd, 0x59, 0x3a, 0x86,
    0x7e, 0xce, 0x06, 0xeb, 0x82, 0x78, 0x57, 0xc7, 0x8d, 0x43, 0xaf, 0xb4, 0x1c, 0xd4, 0x5b, 0xcd,
    0xe2, 0xe9, 0x27, 0x4f, 0xc3, 0x08, 0x72, 0x80, 0xcf, 0xb0, 0xef, 0xf5, 0x28, 0x6d, 0xbe, 0x30,
    0x4d, 0x34, 0x92, 0xd5, 0x0e, 0x3c, 0x22, 0x32, 0xe5, 0xe4, 0xf9, 0x9f, 0xc2, 0xd1, 0x0a, 0x81,
    0x12, 0xe1, 0xee, 0x91, 0x83, 0x76, 0xe3, 0x97, 0xe6, 0x61, 0x8a, 0x17, 0x79, 0xa4, 0xb7, 0xdc,
    0x90, 0x7a, 0x5c, 0x8c, 0x02, 0xa6, 0xca, 0x69, 0xde, 0x50, 0x1a, 0x11, 0x93, 0xb9, 0x52, 0x87,
    0x58, 0xfc, 0xed, 0x1d, 0x37, 0x49, 0x1b, 0x6a, 0xe0, 0x29, 0x33, 0x99, 0xbd, 0x6c, 0xd9, 0x94,
    0xf3, 0x40, 0x54, 0x6f, 0xf0, 0xc6, 0x73, 0xb8, 0xd6, 0x3e, 0x65, 0x18, 0x44, 0x1f, 0xdd, 0x67,
    0x10, 0xf1, 0x0c, 0x19, 0xec, 0xae, 0x03, 0xa1, 0x14, 0x7b, 0xa9, 0x0b, 0xff, 0xf8, 0xa3, 0xc0,
    0xa2, 0x01, 0xf7, 0x2e, 0xbc, 0x24, 0x68, 0x75, 0x0d, 0xfe, 0xba, 0x2f, 0xb5, 0xd0, 0xda, 0x3d
};

static unsigned char comp_high2 [] = {
    0x14, 0x53, 0x0f, 0x56, 0xb3, 0xc8, 0x7a, 0x9c, 0xeb, 0x65, 0x48, 0x17, 0x16, 0x15, 0x9f, 0x02,
    0xcc, 0x54, 0x7c, 0x83, 0x00, 0x0d, 0x0c, 0x0b, 0xa2, 0x62, 0xa8, 0x76, 0xdb, 0xd9, 0xed, 0xc7,
    0xc5, 0xa4, 0xdc, 0xac, 0x85, 0x74, 0xd6, 0xd0, 0xa7, 0x9b, 0xae, 0x9a, 0x96, 0x71, 0x66, 0xc3,
    0x63, 0x99, 0xb8, 0xdd, 0x73, 0x92, 0x8e, 0x84, 0x7d, 0xa5, 0x5e, 0xd1, 0x5d, 0x93, 0xb1, 0x57,
    0x51, 0x50, 0x80, 0x89, 0x52, 0x94, 0x4f, 0x4e, 0x0a, 0x6b, 0xbc, 0x8d, 0x7f, 0x6e, 0x47, 0x46,
    0x41, 0x40, 0x44, 0x01, 0x11, 0xcb, 0x03, 0x3f, 0xf7, 0xf4, 0xe1, 0xa9, 0x8f, 0x3c, 0x3a, 0xf9,
    0xfb, 0xf0, 0x19, 0x30, 0x82, 0x09, 0x2e, 0xc9, 0x9d, 0xa0, 0x86, 0x49, 0xee, 0x6f, 0x4d, 0x6d,
    0xc4, 0x2d, 0x81, 0x34, 0x25, 0x87, 0x1b, 0x88, 0xaa, 0xfc, 0x06, 0xa1, 0x12, 0x38, 0xfd, 0x4c,
    0x42, 0x72, 0x64, 0x13, 0x37, 0x24, 0x6a, 0x75, 0x77, 0x43, 0xff, 0xe6, 0xb4, 0x4b, 0x36, 0x5c,
    0xe4, 0xd8, 0x35, 0x3d, 0x45, 0xb9, 0x2c, 0xec, 0xb7, 0x31, 0x2b, 0x29, 0x07, 0x68, 0xa3, 0x0e,
    0x69, 0x7b, 0x18, 0x9e, 0x21, 0x39, 0xbe, 0x28, 0x1a, 0x5b, 0x78, 0xf5, 0x23, 0xca, 0x2a, 0xb0,
    0xaf, 0x3e, 0xfe, 0x04, 0x8c, 0xe7, 0xe5, 0x98, 0x32, 0x95, 0xd3, 0xf6, 0x4a, 0xe8, 0xa6, 0xea,
    0xe9, 0xf3, 0xd5, 0x2f, 0x70, 0x20, 0xf2, 0x1f, 0x05, 0x67, 0xad, 0x55, 0x10, 0xce, 0xcd, 0xe3,
    0x27, 0x3b, 0xda, 0xba, 0xd7, 0xc2, 0x26, 0xd4, 0x91, 0x1d, 0xd2, 0x1c, 0x22, 0x33, 0xf8, 0xfa,
    0xf1, 0x5a, 0xef, 0xcf, 0x90, 0xb6, 0x8b, 0xb5, 0xbd, 0xc0, 0xbf, 0x08, 0x97, 0x1e, 0x6c, 0xe2,
    0x61, 0xe0, 0xc6, 0xc1, 0x59, 0xab, 0xbb, 0x58, 0xde, 0x5f, 0xdf, 0x60, 0x79, 0x7e, 0xb2, 0x8a
};

int pst_open(pst_file *pf, char *name) {
    int32_t sig;

    unicode_init();

    DEBUG_ENT("pst_open");

    if (!pf) {
        WARN (("cannot be passed a NULL pst_file\n"));
        DEBUG_RET();
        return -1;
    }
    memset(pf, 0, sizeof(*pf));

    if ((pf->fp = fopen(name, "rb")) == NULL) {
        WARN(("cannot open PST file. Error\n"));
        DEBUG_RET();
        return -1;
    }

    // Check pst file magic
    if (pst_getAtPos(pf, 0, &sig, sizeof(sig)) != sizeof(sig)) {
        (void)fclose(pf->fp);
        WARN(("cannot read signature from PST file. Closing on error\n"));
        DEBUG_RET();
        return -1;
    }
    LE32_CPU(sig);
    DEBUG_INFO(("sig = %X\n", sig));
    if (sig != (int32_t)PST_SIGNATURE) {
        (void)fclose(pf->fp);
        WARN(("not a PST file that I know. Closing with error\n"));
        DEBUG_RET();
        return -1;
    }

    // read index type
    (void)pst_getAtPos(pf, INDEX_TYPE_OFFSET, &(pf->ind_type), sizeof(pf->ind_type));
    DEBUG_INFO(("index_type = %i\n", pf->ind_type));
    switch (pf->ind_type) {
        case INDEX_TYPE32 :
            pf->do_read64 = 0;
            break;
        case INDEX_TYPE64 :
            pf->do_read64 = 1;
            break;
        default:
            (void)fclose(pf->fp);
            WARN(("unknown .pst format, possibly newer than Outlook 2003 PST file?\n"));
            DEBUG_RET();
            return -1;
    }

    // read encryption setting
    (void)pst_getAtPos(pf, ENC_TYPE, &(pf->encryption), sizeof(pf->encryption));
    DEBUG_INFO(("encrypt = %i\n", pf->encryption));

    pf->index2_back  = pst_getIntAtPos(pf, SECOND_BACK);
    pf->index2       = pst_getIntAtPos(pf, SECOND_POINTER);
    pf->size         = pst_getIntAtPos(pf, FILE_SIZE_POINTER);
    DEBUG_INFO(("Pointer2 is %#"PRIx64", back pointer2 is %#"PRIx64"\n", pf->index2, pf->index2_back));

    pf->index1_back  = pst_getIntAtPos(pf, INDEX_BACK);
    pf->index1       = pst_getIntAtPos(pf, INDEX_POINTER);
    DEBUG_INFO(("Pointer1 is %#"PRIx64", back pointer2 is %#"PRIx64"\n", pf->index1, pf->index1_back));

    DEBUG_RET();
    return 0;
}


int pst_close(pst_file *pf) {
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
    pst_free_id (pf->i_head);
    pst_free_desc (pf->d_head);
    pst_free_xattrib (pf->x_head);
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
        ret = pst_getDptr(pf, (uint64_t)0x2142);
    } else {
        ret = pst_getDptr(pf, root->message_store->top_of_personal_folder->id);
    }
    DEBUG_RET();
    return ret;
}


size_t pst_attach_to_mem(pst_file *pf, pst_item_attach *attach, char **b){
    size_t size=0;
    pst_index_ll *ptr;
    pst_holder h = {b, NULL, 0};
    DEBUG_ENT("pst_attach_to_mem");
    if (attach->id_val != (uint64_t)-1) {
        ptr = pst_getID(pf, attach->id_val);
        if (ptr) {
            size = pst_ff_getID2data(pf, ptr, &h);
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


size_t pst_attach_to_file(pst_file *pf, pst_item_attach *attach, FILE* fp) {
    pst_index_ll *ptr;
    pst_holder h = {NULL, fp, 0};
    size_t size = 0;
    int32_t x;
    DEBUG_ENT("pst_attach_to_file");
    if (attach->id_val != (uint64_t)-1) {
        ptr = pst_getID(pf, attach->id_val);
        if (ptr) {
            // pst_num_array *list = pst_parse_block(pf, ptr->id, NULL, NULL);
            // DEBUG_WARN(("writing file data attachment\n"));
            // for (x=0; x<list->count_item; x++) {
            //     DEBUG_HEXDUMPC(list->items[x]->data, list->items[x]->size, 0x10);
            //     (void)pst_fwrite(list->items[x]->data, (size_t)1, list->items[x]->size, fp);
            // }
            size = pst_ff_getID2data(pf, ptr, &h);
        } else {
            DEBUG_WARN(("Couldn't find ID pointer. Cannot save attachment to file\n"));
        }
        attach->size = size;
    } else {
        // save the attachment to file
        size = attach->size;
        (void)pst_fwrite(attach->data, (size_t)1, size, fp);
    }
    DEBUG_RET();
    return size;
}


size_t pst_attach_to_file_base64(pst_file *pf, pst_item_attach *attach, FILE* fp) {
    pst_index_ll *ptr;
    pst_holder h = {NULL, fp, 1};
    size_t size = 0;
    int32_t x;
    char *c;
    DEBUG_ENT("pst_attach_to_file_base64");
    if (attach->id_val != (uint64_t)-1) {
        ptr = pst_getID(pf, attach->id_val);
        if (ptr) {
            // pst_num_array *list = pst_parse_block(pf, ptr->id, NULL, NULL);
            // DEBUG_WARN(("writing base64 data attachment\n"));
            // for (x=0; x<list->count_item; x++) {
            //     DEBUG_HEXDUMPC(list->items[x]->data, list->items[x]->size, 0x10);
            //     c = base64_encode(list->items[x]->data, list->items[x]->size);
            //     if (c) {
            //         (void)pst_fwrite(c, (size_t)1, strlen(c), fp);
            //         free(c);    // caught by valgrind
            //     }
            // }
            size = pst_ff_getID2data(pf, ptr, &h);
        } else {
            DEBUG_WARN(("Couldn't find ID pointer. Cannot save attachment to Base64\n"));
        }
        attach->size = size;
    } else {
        // encode the attachment to the file
        c = base64_encode(attach->data, attach->size);
        if (c) {
            (void)pst_fwrite(c, (size_t)1, strlen(c), fp);
            free(c);    // caught by valgrind
        }
        size = attach->size;
    }
    DEBUG_RET();
    return size;
}


int pst_load_index (pst_file *pf) {
    int  x;
    uint64_t y;
    DEBUG_ENT("pst_load_index");
    if (!pf) {
        WARN(("Cannot load index for a NULL pst_file\n"));
        DEBUG_RET();
        return -1;
    }

    x = pst_build_id_ptr(pf, pf->index1, 0, pf->index1_back, 0, UINT64_MAX);
    DEBUG_INDEX(("build id ptr returns %i\n", x));

    y = 0;
    x = pst_build_desc_ptr(pf, pf->index2, 0, pf->index2_back, &y, (uint64_t)0x21, UINT64_MAX);
    DEBUG_INDEX(("build desc ptr returns %i\n", x));

    DEBUG_CODE((void)pst_printDptr(pf, pf->d_head););
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


typedef struct pst_x_attrib {
    uint16_t extended;
    uint16_t zero;
    uint16_t type;
    uint16_t map;
} pst_x_attrib;


int pst_load_extended_attributes(pst_file *pf) {
    // for PST files this will load up ID2 0x61 and check it's "list" attribute.
    pst_desc_ll *p;
    pst_num_array *na;
    pst_index2_ll *id2_head = NULL;
    char *buffer=NULL, *headerbuffer=NULL;
    size_t bsize=0, hsize=0, bptr=0;
    pst_x_attrib xattrib;
    int32_t tint, err=0, x;
    pst_x_attrib_ll *ptr, *p_head=NULL, *p_sh=NULL, *p_sh2=NULL;

    DEBUG_ENT("pst_loadExtendedAttributes");
    p = pst_getDptr(pf, (uint64_t)0x61);
    if (!p) {
        DEBUG_WARN(("Cannot find DescID 0x61 for loading the Extended Attributes\n"));
        DEBUG_RET();
        return 0;
    }

    if (!p->desc) {
        DEBUG_WARN(("desc is NULL for item 0x61. Cannot load Extended Attributes\n"));
        DEBUG_RET();
        return 0;
    }

    if (p->list_index) {
        id2_head = pst_build_id2(pf, p->list_index, NULL);
        pst_printID2ptr(id2_head);
    } else {
        DEBUG_WARN(("Have not been able to fetch any id2 values for item 0x61. Brace yourself!\n"));
    }

    na = pst_parse_block(pf, p->desc->id, id2_head, NULL);
    if (!na) {
        DEBUG_WARN(("Cannot process desc block for item 0x61. Not loading extended Attributes\n"));
        if (id2_head) pst_free_id2(id2_head);
        DEBUG_RET();
        return 0;
    }

    for (x=0; x < na->count_item; x++) {
        if (na->items[x]->id == (uint32_t)0x0003) {
            buffer = na->items[x]->data;
            bsize = na->items[x]->size;
        } else if (na->items[x]->id == (uint32_t)0x0004) {
            headerbuffer = na->items[x]->data;
            hsize = na->items[x]->size;
        } else {
            // leave them null
        }
    }

    if (!buffer) {
        if (na) pst_free_list(na);
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
        ptr = (pst_x_attrib_ll*) xmalloc(sizeof(*ptr));
        memset(ptr, 0, sizeof(*ptr));
        ptr->type = xattrib.type;
        ptr->map  = xattrib.map+0x8000;
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
                wt = (char*) xmalloc((size_t)(tint+2)); // plus 2 for a uni-code zero
                memset(wt, 0, (size_t)(tint+2));
                memcpy(wt, &(headerbuffer[xattrib.extended+sizeof(tint)]), (size_t)tint);
                ptr->data = pst_wide_to_single(wt, (size_t)tint);
                free(wt);
                DEBUG_INDEX(("Read string (converted from UTF-16): %s\n", ptr->data));
            } else {
                DEBUG_INDEX(("Cannot read outside of buffer [%i !< %i]\n", xattrib.extended, hsize));
            }
            ptr->mytype = PST_MAP_HEADER;
        } else {
            // contains the attribute code to map to.
            ptr->data = (uint32_t*)xmalloc(sizeof(uint32_t));
            memset(ptr->data, 0, sizeof(uint32_t));
            *((uint32_t*)ptr->data) = xattrib.extended;
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
    if (id2_head) pst_free_id2(id2_head);
    if (na)       pst_free_list(na);
    pf->x_head = p_head;
    DEBUG_RET();
    return 1;
}


#define ITEM_COUNT_OFFSET32        0x1f0    // count byte
#define LEVEL_INDICATOR_OFFSET32   0x1f3    // node or leaf
#define BACKLINK_OFFSET32          0x1f8    // backlink u1 value
#define ITEM_SIZE32                12
#define DESC_SIZE32                16
#define INDEX_COUNT_MAX32          41       // max active items
#define DESC_COUNT_MAX32           31       // max active items

#define ITEM_COUNT_OFFSET64        0x1e8    // count byte
#define LEVEL_INDICATOR_OFFSET64   0x1eb    // node or leaf
#define BACKLINK_OFFSET64          0x1f8    // backlink u1 value
#define ITEM_SIZE64                24
#define DESC_SIZE64                32
#define INDEX_COUNT_MAX64          20       // max active items
#define DESC_COUNT_MAX64           15       // max active items

#define BLOCK_SIZE                 512      // index blocks
#define DESC_BLOCK_SIZE            512      // descriptor blocks
#define ITEM_COUNT_OFFSET        (size_t)((pf->do_read64) ? ITEM_COUNT_OFFSET64      : ITEM_COUNT_OFFSET32)
#define LEVEL_INDICATOR_OFFSET   (size_t)((pf->do_read64) ? LEVEL_INDICATOR_OFFSET64 : LEVEL_INDICATOR_OFFSET32)
#define BACKLINK_OFFSET          (size_t)((pf->do_read64) ? BACKLINK_OFFSET64        : BACKLINK_OFFSET32)
#define ITEM_SIZE                (size_t)((pf->do_read64) ? ITEM_SIZE64              : ITEM_SIZE32)
#define DESC_SIZE                (size_t)((pf->do_read64) ? DESC_SIZE64              : DESC_SIZE32)
#define INDEX_COUNT_MAX         (int32_t)((pf->do_read64) ? INDEX_COUNT_MAX64        : INDEX_COUNT_MAX32)
#define DESC_COUNT_MAX          (int32_t)((pf->do_read64) ? DESC_COUNT_MAX64         : DESC_COUNT_MAX32)


static size_t pst_decode_desc(pst_file *pf, pst_descn *desc, char *buf);
static size_t pst_decode_desc(pst_file *pf, pst_descn *desc, char *buf) {
    size_t r;
    if (pf->do_read64) {
        DEBUG_INDEX(("Decoding desc64\n"));
        DEBUG_HEXDUMPC(buf, sizeof(pst_descn), 0x10);
        memcpy(desc, buf, sizeof(pst_descn));
        LE64_CPU(desc->d_id);
        LE64_CPU(desc->desc_id);
        LE64_CPU(desc->list_id);
        LE32_CPU(desc->parent_id);
        LE32_CPU(desc->u1);
        r = sizeof(pst_descn);
    }
    else {
        pst_desc32 d32;
        DEBUG_INDEX(("Decoding desc32\n"));
        DEBUG_HEXDUMPC(buf, sizeof(pst_desc32), 0x10);
        memcpy(&d32, buf, sizeof(pst_desc32));
        LE32_CPU(d32.d_id);
        LE32_CPU(d32.desc_id);
        LE32_CPU(d32.list_id);
        LE32_CPU(d32.parent_id);
        desc->d_id      = d32.d_id;
        desc->desc_id   = d32.desc_id;
        desc->list_id   = d32.list_id;
        desc->parent_id = d32.parent_id;
        desc->u1        = 0;
        r = sizeof(pst_desc32);
    }
    return r;
}


static size_t pst_decode_table(pst_file *pf, struct pst_table_ptr_structn *table, char *buf);
static size_t pst_decode_table(pst_file *pf, struct pst_table_ptr_structn *table, char *buf) {
    size_t r;
    if (pf->do_read64) {
        DEBUG_INDEX(("Decoding table64\n"));
        DEBUG_HEXDUMPC(buf, sizeof(struct pst_table_ptr_structn), 0x10);
        memcpy(table, buf, sizeof(struct pst_table_ptr_structn));
        LE64_CPU(table->start);
        LE64_CPU(table->u1);
        LE64_CPU(table->offset);
        r =sizeof(struct pst_table_ptr_structn);
    }
    else {
        struct pst_table_ptr_struct32 t32;
        DEBUG_INDEX(("Decoding table32\n"));
        DEBUG_HEXDUMPC(buf, sizeof( struct pst_table_ptr_struct32), 0x10);
        memcpy(&t32, buf, sizeof(struct pst_table_ptr_struct32));
        LE32_CPU(t32.start);
        LE32_CPU(t32.u1);
        LE32_CPU(t32.offset);
        table->start  = t32.start;
        table->u1     = t32.u1;
        table->offset = t32.offset;
        r = sizeof(struct pst_table_ptr_struct32);
    }
    return r;
}


static size_t pst_decode_index(pst_file *pf, pst_index *index, char *buf);
static size_t pst_decode_index(pst_file *pf, pst_index *index, char *buf) {
    size_t r;
    if (pf->do_read64) {
        DEBUG_INDEX(("Decoding index64\n"));
        DEBUG_HEXDUMPC(buf, sizeof(pst_index), 0x10);
        memcpy(index, buf, sizeof(pst_index));
        LE64_CPU(index->id);
        LE64_CPU(index->offset);
        LE16_CPU(index->size);
        LE16_CPU(index->u0);
        LE16_CPU(index->u1);
        r = sizeof(pst_index);
    } else {
        pst_index32 index32;
        DEBUG_INDEX(("Decoding index32\n"));
        DEBUG_HEXDUMPC(buf, sizeof(pst_index32), 0x10);
        memcpy(&index32, buf, sizeof(pst_index32));
        LE32_CPU(index32.id);
        LE32_CPU(index32.offset);
        LE16_CPU(index32.size);
        LE16_CPU(index32.u1);
        index->id     = index32.id;
        index->offset = index32.offset;
        index->size   = index32.size;
        index->u1     = index32.u1;
        r = sizeof(pst_index32);
    }
    return r;
}


static size_t pst_decode_assoc(pst_file *pf, pst_id2_assoc *assoc, char *buf);
static size_t pst_decode_assoc(pst_file *pf, pst_id2_assoc *assoc, char *buf) {
    size_t r;
    if (pf->do_read64) {
        DEBUG_INDEX(("Decoding assoc64\n"));
        DEBUG_HEXDUMPC(buf, sizeof(pst_id2_assoc), 0x10);
        memcpy(assoc, buf, sizeof(pst_id2_assoc));
        LE32_CPU(assoc->id2);
        LE64_CPU(assoc->id);
        LE64_CPU(assoc->table2);
        r = sizeof(pst_id2_assoc);
    } else {
        pst_id2_assoc32 assoc32;
        DEBUG_INDEX(("Decoding assoc32\n"));
        DEBUG_HEXDUMPC(buf, sizeof(pst_id2_assoc32), 0x10);
        memcpy(&assoc32, buf, sizeof(pst_id2_assoc32));
        LE32_CPU(assoc32.id2);
        LE32_CPU(assoc32.id);
        LE32_CPU(assoc32.table2);
        assoc->id2    = assoc32.id2;
        assoc->id     = assoc32.id;
        assoc->table2 = assoc32.table2;
        r = sizeof(pst_id2_assoc32);
    }
    return r;
}


static size_t pst_decode_type3(pst_file *pf, pst_table3_rec *table3_rec, char *buf);
static size_t pst_decode_type3(pst_file *pf, pst_table3_rec *table3_rec, char *buf) {
    size_t r;
    if (pf->do_read64) {
        DEBUG_INDEX(("Decoding table3 64\n"));
        DEBUG_HEXDUMPC(buf, sizeof(pst_table3_rec), 0x10);
        memcpy(table3_rec, buf, sizeof(pst_table3_rec));
        LE64_CPU(table3_rec->id);
        r = sizeof(pst_table3_rec);
    } else {
        pst_table3_rec32 table3_rec32;
        DEBUG_INDEX(("Decoding table3 32\n"));
        DEBUG_HEXDUMPC(buf, sizeof(pst_table3_rec32), 0x10);
        memcpy(&table3_rec32, buf, sizeof(pst_table3_rec32));
        LE32_CPU(table3_rec32.id);
        table3_rec->id  = table3_rec32.id;
        r = sizeof(pst_table3_rec32);
    }
    return r;
}


int pst_build_id_ptr(pst_file *pf, off_t offset, int32_t depth, uint64_t linku1, uint64_t start_val, uint64_t end_val) {
    struct pst_table_ptr_structn table, table2;
    pst_index_ll *i_ptr=NULL;
    pst_index index;
    int32_t x, item_count;
    uint64_t old = start_val;
    char *buf = NULL, *bptr;

    DEBUG_ENT("pst_build_id_ptr");
    DEBUG_INDEX(("offset %#"PRIx64" depth %i linku1 %#"PRIx64" start %#"PRIx64" end %#"PRIx64"\n", offset, depth, linku1, start_val, end_val));
    if (end_val <= start_val) {
        DEBUG_WARN(("The end value is BEFORE the start value. This function will quit. Soz. [start:%#"PRIx64", end:%#"PRIx64"]\n", start_val, end_val));
        DEBUG_RET();
        return -1;
    }
    DEBUG_INDEX(("Reading index block\n"));
    if (pst_read_block_size(pf, offset, BLOCK_SIZE, &buf) < BLOCK_SIZE) {
        DEBUG_WARN(("Failed to read %i bytes\n", BLOCK_SIZE));
        if (buf) free(buf);
        DEBUG_RET();
        return -1;
    }
    bptr = buf;
    DEBUG_HEXDUMPC(buf, BLOCK_SIZE, ITEM_SIZE32);
    item_count = (int32_t)(unsigned)(buf[ITEM_COUNT_OFFSET]);
    if (item_count > INDEX_COUNT_MAX) {
        DEBUG_WARN(("Item count %i too large, max is %i\n", item_count, INDEX_COUNT_MAX));
        if (buf) free(buf);
        DEBUG_RET();
        return -1;
    }
    index.id = pst_getIntAt(pf, buf+BACKLINK_OFFSET);
    if (index.id != linku1) {
        DEBUG_WARN(("Backlink %#"PRIx64" in this node does not match required %#"PRIx64"\n", index.id, linku1));
        if (buf) free(buf);
        DEBUG_RET();
        return -1;
    }

    if (buf[LEVEL_INDICATOR_OFFSET] == '\0') {
        // this node contains leaf pointers
        x = 0;
        while (x < item_count) {
            bptr += pst_decode_index(pf, &index, bptr);
            x++;
            if (index.id == 0) break;
            DEBUG_INDEX(("[%i]%i Item [id = %#"PRIx64", offset = %#"PRIx64", u1 = %#x, size = %i(%#x)]\n",
                        depth, x, index.id, index.offset, index.u1, index.size, index.size));
            // if (index.id & 0x02) DEBUG_INDEX(("two-bit set!!\n"));
            if ((index.id >= end_val) || (index.id < old)) {
                DEBUG_WARN(("This item isn't right. Must be corruption, or I got it wrong!\n"));
                if (buf) free(buf);
                DEBUG_RET();
                return -1;
            }
            old = index.id;
            if (x == (int32_t)1) {   // first entry
                if ((start_val) && (index.id != start_val)) {
                    DEBUG_WARN(("This item isn't right. Must be corruption, or I got it wrong!\n"));
                    if (buf) free(buf);
                    DEBUG_RET();
                    return -1;
                }
            }
            i_ptr = (pst_index_ll*) xmalloc(sizeof(pst_index_ll));
            i_ptr->id     = index.id;
            i_ptr->offset = index.offset;
            i_ptr->u1     = index.u1;
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
            bptr += pst_decode_table(pf, &table, bptr);
            x++;
            if (table.start == 0) break;
            if (x < item_count) {
                (void)pst_decode_table(pf, &table2, bptr);
            }
            else {
                table2.start = end_val;
            }
            DEBUG_INDEX(("[%i] %i Index Table [start id = %#"PRIx64", u1 = %#"PRIx64", offset = %#"PRIx64", end id = %#"PRIx64"]\n",
                        depth, x, table.start, table.u1, table.offset, table2.start));
            if ((table.start >= end_val) || (table.start < old)) {
                DEBUG_WARN(("This table isn't right. Must be corruption, or I got it wrong!\n"));
                if (buf) free(buf);
                DEBUG_RET();
                return -1;
            }
            old = table.start;
            if (x == (int32_t)1) {  // first entry
                if ((start_val) && (table.start != start_val)) {
                    DEBUG_WARN(("This table isn't right. Must be corruption, or I got it wrong!\n"));
                    if (buf) free(buf);
                    DEBUG_RET();
                    return -1;
                }
            }
            (void)pst_build_id_ptr(pf, table.offset, depth+1, table.u1, table.start, table2.start);
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
    uint64_t parent;
    struct cache_list_node *next;
    struct cache_list_node *prev;
};
static struct cache_list_node *cache_head;
static struct cache_list_node *cache_tail;
static struct cache_list_node *lostfound_head;
static int cache_count;


/**
    add the d_ptr descriptor into the global tree
*/
static void record_descriptor(pst_file *pf, pst_desc_ll *d_ptr, uint64_t parent_id);
static void record_descriptor(pst_file *pf, pst_desc_ll *d_ptr, uint64_t parent_id) {
    struct cache_list_node *lostfound_ptr = NULL;
    struct cache_list_node *cache_ptr     = NULL;
    pst_desc_ll            *parent        = NULL;

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
        pf->d_tail  = d_ptr;
    } else {
        DEBUG_INDEX(("Searching for parent\n"));
        // check in the cache for the parent
        cache_ptr = cache_head;
        while (cache_ptr && (cache_ptr->ptr->id != parent_id)) {
            cache_ptr = cache_ptr->next;
        }
        if (!cache_ptr && (parent = pst_getDptr(pf, parent_id)) == NULL) {
            // check in the lost/found list
            lostfound_ptr = lostfound_head;
            while (lostfound_ptr && (lostfound_ptr->ptr->id != parent_id)) {
                lostfound_ptr = lostfound_ptr->next;
            }
            if (!lostfound_ptr) {
                DEBUG_WARN(("ERROR -- cannot find parent with id %#"PRIx64". Adding to lost/found\n", parent_id));
                lostfound_ptr = (struct cache_list_node*) xmalloc(sizeof(struct cache_list_node));
                lostfound_ptr->prev   = NULL;
                lostfound_ptr->next   = lostfound_head;
                lostfound_ptr->parent = parent_id;
                lostfound_ptr->ptr    = d_ptr;
                lostfound_head = lostfound_ptr;
            } else {
                parent = lostfound_ptr->ptr;
                DEBUG_INDEX(("Found parent (%#"PRIx64") in Lost and Found\n", parent->id));
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
                cache_ptr->ptr  = parent;
                if (cache_head)  cache_head->prev = cache_ptr;
                if (!cache_tail) cache_tail = cache_ptr;
                cache_head = cache_ptr;
                cache_count++;
                if (cache_count > 100) {
                    DEBUG_INDEX(("trimming quick cache\n"));
                    //remove one from the end
                    cache_ptr  = cache_tail;
                    cache_tail = cache_ptr->prev;
                    if (cache_tail) cache_tail->next = NULL;
                    free (cache_ptr);
                    cache_count--;
                }
            }
            DEBUG_INDEX(("Found a parent\n"));
            parent->no_child++;
            d_ptr->parent = parent;
            if (parent->child_tail)  parent->child_tail->next = d_ptr;
            if (!parent->child)      parent->child = d_ptr;
            d_ptr->prev = parent->child_tail;
            parent->child_tail = d_ptr;
        }
    }
}

int pst_build_desc_ptr (pst_file *pf, off_t offset, int32_t depth, uint64_t linku1, uint64_t *high_id, uint64_t start_val, uint64_t end_val) {
    struct pst_table_ptr_structn table, table2;
    pst_descn desc_rec;
    pst_desc_ll *d_ptr=NULL, *parent=NULL;
    int32_t x, item_count;
    uint64_t old = start_val;
    char *buf = NULL, *bptr;
    struct cache_list_node *cache_ptr = NULL;
    struct cache_list_node *lostfound_ptr = NULL;
    struct cache_list_node *lostfound_shd = NULL;
    struct cache_list_node *lostfound_tmp = NULL;

    if (depth == 0) {
        // initialize the linked list and lost/found list.
        cache_head     = NULL;
        cache_tail     = NULL;
        lostfound_head = NULL;
        cache_count    = 0;
    }

    DEBUG_ENT("pst_build_desc_ptr");
    DEBUG_INDEX(("offset %#"PRIx64" depth %i linku1 %#"PRIx64" start %#"PRIx64" end %#"PRIx64"\n", offset, depth, linku1, start_val, end_val));
    if (end_val <= start_val) {
        DEBUG_WARN(("The end value is BEFORE the start value. This function will quit. Soz. [start:%#"PRIx64", end:%#"PRIx64"]\n", start_val, end_val));
        DEBUG_RET();
        return -1;
    }
    DEBUG_INDEX(("Reading desc block\n"));
    if (pst_read_block_size(pf, offset, DESC_BLOCK_SIZE, &buf) < DESC_BLOCK_SIZE) {
        DEBUG_WARN(("Failed to read %i bytes\n", DESC_BLOCK_SIZE));
        if (buf) free(buf);
        DEBUG_RET();
        return -1;
    }
    bptr = buf;
    item_count = (int32_t)(unsigned)(buf[ITEM_COUNT_OFFSET]);

    desc_rec.d_id = pst_getIntAt(pf, buf+BACKLINK_OFFSET);
    if (desc_rec.d_id != linku1) {
        DEBUG_WARN(("Backlink %#"PRIx64" in this node does not match required %#"PRIx64"\n", desc_rec.d_id, linku1));
        if (buf) free(buf);
        DEBUG_RET();
        return -1;
    }
    if (buf[LEVEL_INDICATOR_OFFSET] == '\0') {
        // this node contains leaf pointers
        DEBUG_HEXDUMPC(buf, DESC_BLOCK_SIZE, DESC_SIZE32);
        if (item_count > DESC_COUNT_MAX) {
            DEBUG_WARN(("Item count %i too large, max is %i\n", item_count, DESC_COUNT_MAX));
            if (buf) free(buf);
            DEBUG_RET();
            return -1;
        }
        x = 0;
        while (x < item_count) {
            bptr += pst_decode_desc(pf, &desc_rec, bptr);
            x++;
            if (desc_rec.d_id == 0) break;
            DEBUG_INDEX(("[%i] Item(%#x) = [d_id = %#"PRIx64", desc_id = %#"PRIx64", list_id = %#"PRIx64", parent_id = %#x]\n",
                        depth, x, desc_rec.d_id, desc_rec.desc_id, desc_rec.list_id, desc_rec.parent_id));
            if ((desc_rec.d_id >= end_val) || (desc_rec.d_id < old)) {
                DEBUG_WARN(("This item isn't right. Must be corruption, or I got it wrong!\n"));
                DEBUG_HEXDUMPC(buf, DESC_BLOCK_SIZE, 16);
                if (buf) free(buf);
                DEBUG_RET();
                return -1;
            }
            old = desc_rec.d_id;
            if (x == (int32_t)1) {   // first entry
                if (start_val && (desc_rec.d_id != start_val)) {
                    DEBUG_WARN(("This item isn't right. Must be corruption, or I got it wrong!\n"));
                    if (buf) free(buf);
                    DEBUG_RET();
                    return -1;
                }
            }
            // When duplicates found, just update the info.... perhaps this is correct functionality
            DEBUG_INDEX(("Searching for existing record\n"));
            if (desc_rec.d_id <= *high_id && (d_ptr = pst_getDptr(pf, desc_rec.d_id))) {
                uint64_t bigzero = 0;
                DEBUG_INDEX(("Updating Existing Values\n"));
                d_ptr->list_index = pst_getID(pf, desc_rec.list_id);
                d_ptr->desc = pst_getID(pf, desc_rec.desc_id);
                DEBUG_INDEX(("\tdesc = %#"PRIx64"\tlist_index=%#"PRIx64"\n",
                        (d_ptr->desc==NULL       ? bigzero : d_ptr->desc->id),
                        (d_ptr->list_index==NULL ? bigzero : d_ptr->list_index->id)));
                if (d_ptr->parent && desc_rec.parent_id != d_ptr->parent->id) {
                    DEBUG_INDEX(("WARNING -- Parent of record has changed. Moving it\n"));
                    //hmmm, we must move the record.
                    // first we must remove from current location
                    //   change previous record to point next to our next
                    //     if no previous, then use parent's child
                    //     if no parent then change pf->d_head;
                    //   change next's prev to our prev
                    //     if no next then change parent's child_tail
                    //     if no parent then change pf->d_tail
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
                    record_descriptor(pf, d_ptr, desc_rec.parent_id);   // add to the global tree
                }
            } else {
                if (*high_id < desc_rec.d_id) {
                    DEBUG_INDEX(("Updating New High\n"));
                    *high_id = desc_rec.d_id;
                }
                DEBUG_INDEX(("New Record\n"));
                d_ptr             = (pst_desc_ll*) xmalloc(sizeof(pst_desc_ll));
                d_ptr->id         = desc_rec.d_id;
                d_ptr->list_index = pst_getID(pf, desc_rec.list_id);
                d_ptr->desc       = pst_getID(pf, desc_rec.desc_id);
                d_ptr->prev       = NULL;
                d_ptr->next       = NULL;
                d_ptr->parent     = NULL;
                d_ptr->child      = NULL;
                d_ptr->child_tail = NULL;
                d_ptr->no_child   = 0;
                record_descriptor(pf, d_ptr, desc_rec.parent_id);   // add to the global tree

            }
            // check here to see if d_ptr is the parent of any of the items in the lost / found list
            lostfound_ptr = lostfound_head;
            lostfound_shd = NULL;
            while (lostfound_ptr) {
                if (lostfound_ptr->parent == d_ptr->id) {
                    DEBUG_INDEX(("Found a child  (%#"PRIx64") of the current record. Joining to main structure.\n", lostfound_ptr->ptr->id));
                    parent = d_ptr;
                    d_ptr = lostfound_ptr->ptr;
                    parent->no_child++;
                    d_ptr->parent = parent;
                    if (parent->child_tail) parent->child_tail->next = d_ptr;
                    if (!parent->child)     parent->child = d_ptr;
                    d_ptr->prev = parent->child_tail;
                    parent->child_tail = d_ptr;
                    if (!lostfound_shd) lostfound_head      = lostfound_ptr->next;
                    else                lostfound_shd->next = lostfound_ptr->next;
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
        DEBUG_HEXDUMPC(buf, DESC_BLOCK_SIZE, ITEM_SIZE32);
        if (item_count > INDEX_COUNT_MAX) {
            DEBUG_WARN(("Item count %i too large, max is %i\n", item_count, INDEX_COUNT_MAX));
            if (buf) free(buf);
            DEBUG_RET();
            return -1;
        }
        x = 0;
        while (x < item_count) {
            bptr += pst_decode_table(pf, &table, bptr);
            x++;
            if (table.start == 0) break;
            if (x < item_count) {
                (void)pst_decode_table(pf, &table2, bptr);
            }
            else {
                table2.start = end_val;
            }
            DEBUG_INDEX(("[%i] %i Descriptor Table [start id = %#"PRIx64", u1 = %#"PRIx64", offset = %#"PRIx64", end id = %#"PRIx64"]\n",
                        depth, x, table.start, table.u1, table.offset, table2.start));
            if ((table.start >= end_val) || (table.start < old)) {
                DEBUG_WARN(("This table isn't right. Must be corruption, or I got it wrong!\n"));
                if (buf) free(buf);
                DEBUG_RET();
                return -1;
            }
            old = table.start;
            if (x == (int32_t)1) {   // first entry
                if (start_val && (table.start != start_val)) {
                    DEBUG_WARN(("This table isn't right. Must be corruption, or I got it wrong!\n"));
                    if (buf) free(buf);
                    DEBUG_RET();
                    return -1;
                }
            }
            (void)pst_build_desc_ptr(pf, table.offset, depth+1, table.u1, high_id, table.start, table2.start);
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
            WARN(("unused lost/found item with parent %#"PRIx64"))", lostfound_head->parent));
            free(lostfound_head);
            lostfound_head = lostfound_ptr;
        }
    }
    if (buf) free(buf);
    DEBUG_RET();
    return 0;
}


pst_item* pst_parse_item(pst_file *pf, pst_desc_ll *d_ptr) {
    pst_num_array * list;
    pst_index2_ll *id2_head = NULL;
    pst_index_ll *id_ptr = NULL;
    pst_item *item = NULL;
    pst_item_attach *attach = NULL;
    int32_t x;
    DEBUG_ENT("pst_parse_item");
    if (!d_ptr) {
        DEBUG_WARN(("you cannot pass me a NULL! I don't want it!\n"));
        DEBUG_RET();
        return NULL;
    }

    if (!d_ptr->desc) {
        DEBUG_WARN(("why is d_ptr->desc == NULL? I don't want to do anything else with this record\n"));
        DEBUG_RET();
        return NULL;
    }

    if (d_ptr->list_index) {
        id2_head = pst_build_id2(pf, d_ptr->list_index, NULL);
        (void)pst_printID2ptr(id2_head);
    } else {
        DEBUG_WARN(("Have not been able to fetch any id2 values for this item. Brace yourself!\n"));
    }

    list = pst_parse_block(pf, d_ptr->desc->id, id2_head, NULL);
    if (!list) {
        DEBUG_WARN(("pst_parse_block() returned an error for d_ptr->desc->id [%#"PRIx64"]\n", d_ptr->desc->id));
        if (id2_head) pst_free_id2(id2_head);
        DEBUG_RET();
        return NULL;
    }

    item = (pst_item*) xmalloc(sizeof(pst_item));
    memset(item, 0, sizeof(pst_item));

    if (pst_process(list, item, NULL)) {
        DEBUG_WARN(("pst_process() returned non-zero value. That is an error\n"));
        if (item)     pst_freeItem(item);
        if (list)     pst_free_list(list);
        if (id2_head) pst_free_id2(id2_head);
        DEBUG_RET();
        return NULL;
    }
    if (list) pst_free_list(list);
    list = NULL; //pst_process will free the items in the list

    if ((id_ptr = pst_getID2(id2_head, (uint64_t)0x671))) {
        // attachments exist - so we will process them
        while (item->attach) {
            attach = item->attach->next;
            free(item->attach);
            item->attach = attach;
        }

        DEBUG_EMAIL(("ATTACHMENT processing attachment\n"));
        if ((list = pst_parse_block(pf, id_ptr->id, id2_head, NULL)) == NULL) {
            DEBUG_WARN(("ERROR error processing main attachment record\n"));
            //if (item) pst_freeItem(item);
            if (id2_head) pst_free_id2(id2_head);
            DEBUG_RET();
            //return NULL;
            return item;
        }
        else {
            for (x=0; x < list->count_array; x++) {
                attach = (pst_item_attach*) xmalloc(sizeof(pst_item_attach));
                memset(attach, 0, sizeof(pst_item_attach));
                attach->next = item->attach;
                item->attach = attach;
            }

            if (pst_process(list, item, item->attach)) {
                DEBUG_WARN(("ERROR pst_process() failed with attachments\n"));
                if (item)     pst_freeItem(item);
                if (list)     pst_free_list(list);
                if (id2_head) pst_free_id2(id2_head);
                DEBUG_RET();
                return NULL;
            }
            if (list) pst_free_list(list);
            list = NULL;

            // now we will have initial information of each attachment stored in item->attach...
            // we must now read the secondary record for each based on the id2 val associated with
            // each attachment
            attach = item->attach;
            while (attach) {
                DEBUG_WARN(("initial attachment id2 %#"PRIx64"\n", attach->id2_val));
                if ((id_ptr = pst_getID2(id2_head, attach->id2_val))) {
                    DEBUG_WARN(("initial attachment id2 found id %#"PRIx64"\n", id_ptr->id));
                    // id_ptr is a record describing the attachment
                    // we pass NULL instead of id2_head cause we don't want it to
                    // load all the extra stuff here.
                    if ((list = pst_parse_block(pf, id_ptr->id, NULL, NULL)) == NULL) {
                        DEBUG_WARN(("ERROR error processing an attachment record\n"));
                        attach = attach->next;
                        continue;
                    }
                    if (pst_process(list, item, attach)) {
                        DEBUG_WARN(("ERROR pst_process() failed with an attachment\n"));
                        if (list) pst_free_list(list);
                        list = NULL;
                        attach = attach->next;
                        continue;
                    }
                    if (list) pst_free_list(list);
                    list = NULL;
                    id_ptr = pst_getID2(id2_head, attach->id2_val);
                    if (id_ptr) {
                        DEBUG_WARN(("second pass attachment updating id2 found id %#"PRIx64"\n", id_ptr->id));
                        // id2_val has been updated to the ID2 value of the datablock containing the
                        // attachment data
                        attach->id_val = id_ptr->id;
                    } else {
                        DEBUG_WARN(("have not located the correct value for the attachment [%#"PRIx64"]\n", attach->id2_val));
                    }
                } else {
                    DEBUG_WARN(("ERROR cannot locate id2 value %#"PRIx64"\n", attach->id2_val));
                }
                attach = attach->next;
            }
        }
    }

    if (id2_head) pst_free_id2(id2_head);
    DEBUG_RET();
    return item;
}


static void freeall(pst_subblocks *subs, pst_block_offset_pointer *p1,
                                         pst_block_offset_pointer *p2,
                                         pst_block_offset_pointer *p3,
                                         pst_block_offset_pointer *p4,
                                         pst_block_offset_pointer *p5,
                                         pst_block_offset_pointer *p6,
                                         pst_block_offset_pointer *p7);
static void freeall(pst_subblocks *subs, pst_block_offset_pointer *p1,
                                         pst_block_offset_pointer *p2,
                                         pst_block_offset_pointer *p3,
                                         pst_block_offset_pointer *p4,
                                         pst_block_offset_pointer *p5,
                                         pst_block_offset_pointer *p6,
                                         pst_block_offset_pointer *p7) {
    size_t i;
    for (i=0; i<subs->subblock_count; i++) {
        if (subs->subs[i].buf) free(subs->subs[i].buf);
    }
    free(subs->subs);
    if (p1->needfree) free(p1->from);
    if (p2->needfree) free(p2->from);
    if (p3->needfree) free(p3->from);
    if (p4->needfree) free(p4->from);
    if (p5->needfree) free(p5->from);
    if (p6->needfree) free(p6->from);
    if (p7->needfree) free(p7->from);
}


pst_num_array * pst_parse_block(pst_file *pf, uint64_t block_id, pst_index2_ll *i2_head, pst_num_array *na_head) {
    char  *buf       = NULL;
    size_t read_size = 0;
    pst_subblocks  subblocks;
    pst_num_array *na_ptr = NULL;
    pst_block_offset_pointer block_offset1;
    pst_block_offset_pointer block_offset2;
    pst_block_offset_pointer block_offset3;
    pst_block_offset_pointer block_offset4;
    pst_block_offset_pointer block_offset5;
    pst_block_offset_pointer block_offset6;
    pst_block_offset_pointer block_offset7;
    int32_t  x;
    int      num_recs;
    int      count_rec;
    int32_t  num_list;
    int32_t  cur_list;
    int      block_type;
    uint32_t rec_size = 0;
    char*    list_start;
    char*    fr_ptr;
    char*    to_ptr;
    char*    ind2_end = NULL;
    char*    ind2_ptr = NULL;
    pst_x_attrib_ll *mapptr;
    pst_block_hdr    block_hdr;
    pst_table3_rec   table3_rec;  //for type 3 (0x0101) blocks

    struct {
        unsigned char seven_c;
        unsigned char item_count;
        uint16_t u1;
        uint16_t u2;
        uint16_t u3;
        uint16_t rec_size;
        uint32_t b_five_offset;
        uint32_t ind2_offset;
        uint16_t u7;
        uint16_t u8;
    } seven_c_blk;

    struct _type_d_rec {
        uint32_t id;
        uint32_t u1;
    } * type_d_rec;

    struct {
        uint16_t type;
        uint16_t ref_type;
        uint32_t value;
    } table_rec;    //for type 1 (0xBCEC) blocks

    struct {
        uint16_t ref_type;
        uint16_t type;
        uint16_t ind2_off;
        uint8_t  size;
        uint8_t  slot;
    } table2_rec;   //for type 2 (0x7CEC) blocks

    DEBUG_ENT("pst_parse_block");
    if ((read_size = pst_ff_getIDblock_dec(pf, block_id, &buf)) == 0) {
        WARN(("Error reading block id %#"PRIx64"\n", block_id));
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
    DEBUG_EMAIL(("block header (index_offset=%#hx, type=%#hx, offset=%#hx)\n", block_hdr.index_offset, block_hdr.type, block_hdr.offset));

    if (block_hdr.index_offset == (uint16_t)0x0101) { //type 3
        size_t i;
        char *b_ptr = buf + 8;
        subblocks.subblock_count = block_hdr.type;
        subblocks.subs = malloc(sizeof(pst_subblock) * subblocks.subblock_count);
        for (i=0; i<subblocks.subblock_count; i++) {
            b_ptr += pst_decode_type3(pf, &table3_rec, b_ptr);
            subblocks.subs[i].buf       = NULL;
            subblocks.subs[i].read_size = pst_ff_getIDblock_dec(pf, table3_rec.id, &subblocks.subs[i].buf);
            if (subblocks.subs[i].buf) {
                memcpy(&block_hdr, subblocks.subs[i].buf, sizeof(block_hdr));
                LE16_CPU(block_hdr.index_offset);
                subblocks.subs[i].i_offset = block_hdr.index_offset;
            }
            else {
                subblocks.subs[i].read_size = 0;
                subblocks.subs[i].i_offset  = 0;
            }
        }
        free(buf);
        memcpy(&block_hdr, subblocks.subs[0].buf, sizeof(block_hdr));
        LE16_CPU(block_hdr.index_offset);
        LE16_CPU(block_hdr.type);
        LE32_CPU(block_hdr.offset);
        DEBUG_EMAIL(("block header (index_offset=%#hx, type=%#hx, offset=%#hx)\n", block_hdr.index_offset, block_hdr.type, block_hdr.offset));
    }
    else {
        // setup the subblock descriptors, but we only have one block
        subblocks.subblock_count = (size_t)1;
        subblocks.subs = malloc(sizeof(pst_subblock));
        subblocks.subs[0].buf       = buf;
        subblocks.subs[0].read_size = read_size;
        subblocks.subs[0].i_offset  = block_hdr.index_offset;
    }

    if (block_hdr.type == (uint16_t)0xBCEC) { //type 1
        block_type = 1;

        if (pst_getBlockOffsetPointer(pf, i2_head, &subblocks, block_hdr.offset, &block_offset1)) {
            DEBUG_WARN(("internal error (bc.b5 offset %#x) in reading block id %#x\n", block_hdr.offset, block_id));
            freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
            DEBUG_RET();
            return NULL;
        }
        memcpy(&table_rec, block_offset1.from, sizeof(table_rec));
        LE16_CPU(table_rec.type);
        LE16_CPU(table_rec.ref_type);
        LE32_CPU(table_rec.value);
        DEBUG_EMAIL(("table_rec (type=%#hx, ref_type=%#hx, value=%#x)\n", table_rec.type, table_rec.ref_type, table_rec.value));

        if ((table_rec.type != (uint16_t)0x02B5) || (table_rec.ref_type != 6)) {
            WARN(("Unknown second block constant - %#hx %#hx for id %#"PRIx64"\n", table_rec.type, table_rec.ref_type, block_id));
            freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
            DEBUG_RET();
            return NULL;
        }

        if (pst_getBlockOffsetPointer(pf, i2_head, &subblocks, table_rec.value, &block_offset2)) {
            DEBUG_WARN(("internal error (bc.b5.desc offset) in reading block id %#x\n", table_rec.value, block_id));
            freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
            DEBUG_RET();
            return NULL;
        }
        list_start = block_offset2.from;
        to_ptr     = block_offset2.to;
        num_list = (to_ptr - list_start)/sizeof(table_rec);
        num_recs = 1; // only going to be one object in these blocks
    }
    else if (block_hdr.type == (uint16_t)0x7CEC) { //type 2
        block_type = 2;

        if (pst_getBlockOffsetPointer(pf, i2_head, &subblocks, block_hdr.offset, &block_offset3)) {
            DEBUG_WARN(("internal error (7c.7c offset %#x) in reading block id %#x\n", block_hdr.offset, block_id));
            freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
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
            freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
            DEBUG_RET();
            return NULL;
        }

        rec_size = seven_c_blk.rec_size;
        num_list = (int32_t)(unsigned)seven_c_blk.item_count;

        if (pst_getBlockOffsetPointer(pf, i2_head, &subblocks, seven_c_blk.b_five_offset, &block_offset4)) {
            DEBUG_WARN(("internal error (7c.b5 offset %#x) in reading block id %#x\n", seven_c_blk.b_five_offset, block_id));
            freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
            DEBUG_RET();
            return NULL;
        }
        memcpy(&table_rec, block_offset4.from, sizeof(table_rec));
        LE16_CPU(table_rec.type);
        LE16_CPU(table_rec.ref_type);
        LE32_CPU(table_rec.value);
        DEBUG_EMAIL(("table_rec (type=%#hx, ref_type=%#hx, value=%#x)\n", table_rec.type, table_rec.ref_type, table_rec.value));

        if (table_rec.type != (uint16_t)0x04B5) { // different constant than a type 1 record
            WARN(("Unknown second block constant - %#hx for id %#"PRIx64"\n", table_rec.type, block_id));
            freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
            DEBUG_RET();
            return NULL;
        }

        if (pst_getBlockOffsetPointer(pf, i2_head, &subblocks, table_rec.value, &block_offset5)) {
            DEBUG_WARN(("internal error (7c.b5.desc offset %#x) in reading block id %#"PRIx64"\n", table_rec.value, block_id));
            freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
            DEBUG_RET();
            return NULL;
        }

        // this will give the number of records in this block
        num_recs = (block_offset5.to - block_offset5.from) / (4 + table_rec.ref_type);

        if (pst_getBlockOffsetPointer(pf, i2_head, &subblocks, seven_c_blk.ind2_offset, &block_offset6)) {
            DEBUG_WARN(("internal error (7c.ind2 offset %#x) in reading block id %#x\n", seven_c_blk.ind2_offset, block_id));
            freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
            DEBUG_RET();
            return NULL;
        }
        ind2_ptr = block_offset6.from;
        ind2_end = block_offset6.to;
    }
    else {
        WARN(("ERROR: Unknown block constant - %#hx for id %#"PRIx64"\n", block_hdr.type, block_id));
        freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
        DEBUG_RET();
        return NULL;
    }

    DEBUG_EMAIL(("Mallocing number of records %i\n", num_recs));
    for (count_rec=0; count_rec<num_recs; count_rec++) {
        na_ptr = (pst_num_array*) xmalloc(sizeof(pst_num_array));
        memset(na_ptr, 0, sizeof(pst_num_array));
        na_ptr->next = na_head;
        na_head = na_ptr;
        // allocate an array of count num_recs to contain sizeof(pst_num_item)
        na_ptr->items       = (pst_num_item**) xmalloc(sizeof(pst_num_item)*num_list);
        na_ptr->count_item  = num_list;
        na_ptr->orig_count  = num_list;
        na_ptr->count_array = (int32_t)num_recs; // each record will have a record of the total number of records
        for (x=0; x<num_list; x++) na_ptr->items[x] = NULL;
        x = 0;

        DEBUG_EMAIL(("going to read %i (%#x) items\n", na_ptr->count_item, na_ptr->count_item));

        fr_ptr = list_start; // initialize fr_ptr to the start of the list.
        for (cur_list=0; cur_list<num_list; cur_list++) { //we will increase fr_ptr as we progress through index
            char* value_pointer = NULL;     // needed for block type 2 with values larger than 4 bytes
            size_t value_size = 0;
            if (block_type == 1) {
                memcpy(&table_rec, fr_ptr, sizeof(table_rec));
                LE16_CPU(table_rec.type);
                LE16_CPU(table_rec.ref_type);
                //LE32_CPU(table_rec.value);    // done later, some may be order invariant
                fr_ptr += sizeof(table_rec);
            } else if (block_type == 2) {
                // we will copy the table2_rec values into a table_rec record so that we can keep the rest of the code
                memcpy(&table2_rec, fr_ptr, sizeof(table2_rec));
                LE16_CPU(table2_rec.ref_type);
                LE16_CPU(table2_rec.type);
                LE16_CPU(table2_rec.ind2_off);

                // table_rec and table2_rec are arranged differently, so assign the values across
                table_rec.type     = table2_rec.type;
                table_rec.ref_type = table2_rec.ref_type;
                table_rec.value    = 0;
                if ((ind2_end - ind2_ptr) >= (int)(table2_rec.ind2_off + table2_rec.size)) {
                    size_t n = table2_rec.size;
                    size_t m = sizeof(table_rec.value);
                    if (n <= m) {
                        memcpy(&table_rec.value, ind2_ptr + table2_rec.ind2_off, n);
                    }
                    else {
                        value_pointer = ind2_ptr + table2_rec.ind2_off;
                        value_size    = n;
                    }
                    //LE32_CPU(table_rec.value);    // done later, some may be order invariant
                }
                else {
                    DEBUG_WARN (("Trying to read outside buffer, buffer size %#x, offset %#x, data size %#x\n",
                                read_size, ind2_end-ind2_ptr+table2_rec.ind2_off, table2_rec.size));
                }
                fr_ptr += sizeof(table2_rec);
            } else {
                WARN(("Missing code for block_type %i\n", block_type));
                freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
                if (na_head) pst_free_list(na_head);
                DEBUG_RET();
                return NULL;
            }
            DEBUG_EMAIL(("reading block %i (type=%#x, ref_type=%#x, value=%#x)\n",
                x, table_rec.type, table_rec.ref_type, table_rec.value));

            if (!na_ptr->items[x]) {
                na_ptr->items[x] = (pst_num_item*) xmalloc(sizeof(pst_num_item));
            }
            memset(na_ptr->items[x], 0, sizeof(pst_num_item)); //init it

            // check here to see if the id of the attribute is a mapped one
            mapptr = pf->x_head;
            while (mapptr && (mapptr->map < table_rec.type)) mapptr = mapptr->next;
            if (mapptr && (mapptr->map == table_rec.type)) {
                if (mapptr->mytype == PST_MAP_ATTRIB) {
                    na_ptr->items[x]->id = *((uint32_t*)mapptr->data);
                    DEBUG_EMAIL(("Mapped attrib %#x to %#x\n", table_rec.type, na_ptr->items[x]->id));
                } else if (mapptr->mytype == PST_MAP_HEADER) {
                    DEBUG_EMAIL(("Internet Header mapping found %#x\n", table_rec.type));
                    na_ptr->items[x]->id = (uint32_t)PST_ATTRIB_HEADER;
                    na_ptr->items[x]->extra = mapptr->data;
                }
                else {
                    DEBUG_WARN(("Missing assertion failure\n"));
                    // nothing, should be assertion failure here
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

            if (table_rec.ref_type == (uint16_t)0x0002 ||
                table_rec.ref_type == (uint16_t)0x0003 ||
                table_rec.ref_type == (uint16_t)0x000b) {
                //contains 32 bits of data
                na_ptr->items[x]->size = sizeof(int32_t);
                na_ptr->items[x]->type = table_rec.ref_type;
                na_ptr->items[x]->data = xmalloc(sizeof(int32_t));
                memcpy(na_ptr->items[x]->data, &(table_rec.value), sizeof(int32_t));
                // are we missing an LE32_CPU() call here? table_rec.value is still
                // in the original order.

            } else if (table_rec.ref_type == (uint16_t)0x0005 ||
                       table_rec.ref_type == (uint16_t)0x000d ||
                       table_rec.ref_type == (uint16_t)0x0014 ||
                       table_rec.ref_type == (uint16_t)0x001e ||
                       table_rec.ref_type == (uint16_t)0x001f ||
                       table_rec.ref_type == (uint16_t)0x0040 ||
                       table_rec.ref_type == (uint16_t)0x0048 ||
                       table_rec.ref_type == (uint16_t)0x0102 ||
                       table_rec.ref_type == (uint16_t)0x1003 ||
                       table_rec.ref_type == (uint16_t)0x1014 ||
                       table_rec.ref_type == (uint16_t)0x101e ||
                       table_rec.ref_type == (uint16_t)0x101f ||
                       table_rec.ref_type == (uint16_t)0x1102) {
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
                else if (pst_getBlockOffsetPointer(pf, i2_head, &subblocks, table_rec.value, &block_offset7)) {
                    if ((table_rec.value & 0xf) == (uint32_t)0xf) {
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
                    value_size = (size_t)(block_offset7.to - block_offset7.from);
                    na_ptr->items[x]->size = value_size;
                    na_ptr->items[x]->type = table_rec.ref_type;
                    na_ptr->items[x]->data = xmalloc(value_size+1);
                    memcpy(na_ptr->items[x]->data, block_offset7.from, value_size);
                    na_ptr->items[x]->data[value_size] = '\0';  // it might be a string, null terminate it.
                }
                if (table_rec.ref_type == (uint16_t)0xd) {
                    // there is still more to do for the type of 0xD embedded objects
                    type_d_rec = (struct _type_d_rec*) na_ptr->items[x]->data;
                    LE32_CPU(type_d_rec->id);
                    na_ptr->items[x]->size = pst_ff_getID2block(pf, type_d_rec->id, i2_head, &(na_ptr->items[x]->data));
                    if (!na_ptr->items[x]->size){
                        DEBUG_WARN(("not able to read the ID2 data. Setting to be read later. %#x\n", type_d_rec->id));
                        na_ptr->items[x]->type = type_d_rec->id;    // fetch before freeing data, alias pointer
                        free(na_ptr->items[x]->data);
                        na_ptr->items[x]->data = NULL;
                    }
                }
                if (table_rec.ref_type == (uint16_t)0x1f) {
                    // there is more to do for the type 0x1f unicode strings
                    static vbuf *strbuf = NULL;
                    static vbuf *unibuf = NULL;
                    if (!strbuf) strbuf=vballoc((size_t)1024);
                    if (!unibuf) unibuf=vballoc((size_t)1024);

                    // splint barfed on the following lines
                    //VBUF_STATIC(strbuf, 1024);
                    //VBUF_STATIC(unibuf, 1024);

                    //need UTF-16 zero-termination
                    vbset(strbuf, na_ptr->items[x]->data, na_ptr->items[x]->size);
                    vbappend(strbuf, "\0\0", (size_t)2);
                    DEBUG_INDEX(("Iconv in:\n"));
                    DEBUG_HEXDUMPC(strbuf->b, strbuf->dlen, 0x10);
                    (void)vb_utf16to8(unibuf, strbuf->b, strbuf->dlen);
                    free(na_ptr->items[x]->data);
                    na_ptr->items[x]->size = unibuf->dlen;
                    na_ptr->items[x]->data = xmalloc(unibuf->dlen);
                    memcpy(na_ptr->items[x]->data, unibuf->b, unibuf->dlen);
                    DEBUG_INDEX(("Iconv out:\n"));
                    DEBUG_HEXDUMPC(na_ptr->items[x]->data, na_ptr->items[x]->size, 0x10);
                }
                if (na_ptr->items[x]->type == 0) na_ptr->items[x]->type = table_rec.ref_type;
            } else {
                WARN(("ERROR Unknown ref_type %#hx\n", table_rec.ref_type));
                freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
                if (na_head) pst_free_list(na_head);
                DEBUG_RET();
                return NULL;
            }
            x++;
        }
        DEBUG_EMAIL(("increasing ind2_ptr by %i [%#x] bytes. Was %#x, Now %#x\n", rec_size, rec_size, ind2_ptr, ind2_ptr+rec_size));
        ind2_ptr += rec_size;
    }
    freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
    DEBUG_RET();
    return na_head;
}


// This version of free does NULL check first
#define SAFE_FREE(x) {if (x) free(x);}


// check if item->email is NULL, and init if so
#define MALLOC_EMAIL(x)        { if (!x->email)         { x->email         = (pst_item_email*)         xmalloc(sizeof(pst_item_email));         memset(x->email,         0, sizeof(pst_item_email)        );} }
#define MALLOC_FOLDER(x)       { if (!x->folder)        { x->folder        = (pst_item_folder*)        xmalloc(sizeof(pst_item_folder));        memset(x->folder,        0, sizeof(pst_item_folder)       );} }
#define MALLOC_CONTACT(x)      { if (!x->contact)       { x->contact       = (pst_item_contact*)       xmalloc(sizeof(pst_item_contact));       memset(x->contact,       0, sizeof(pst_item_contact)      );} }
#define MALLOC_MESSAGESTORE(x) { if (!x->message_store) { x->message_store = (pst_item_message_store*) xmalloc(sizeof(pst_item_message_store)); memset(x->message_store, 0, sizeof(pst_item_message_store));} }
#define MALLOC_JOURNAL(x)      { if (!x->journal)       { x->journal       = (pst_item_journal*)       xmalloc(sizeof(pst_item_journal));       memset(x->journal,       0, sizeof(pst_item_journal)      );} }
#define MALLOC_APPOINTMENT(x)  { if (!x->appointment)   { x->appointment   = (pst_item_appointment*)   xmalloc(sizeof(pst_item_appointment));   memset(x->appointment,   0, sizeof(pst_item_appointment)  );} }
// malloc space and copy the current item's data null terminated
#define LIST_COPY(targ, type) {                               \
    targ = type realloc(targ, list->items[x]->size+1);        \
    memcpy(targ, list->items[x]->data, list->items[x]->size); \
    memset(((char*)targ)+list->items[x]->size, 0, (size_t)1); \
}
// malloc space and copy the item filetime
#define LIST_COPY_TIME(targ) {                                \
    targ = (FILETIME*) realloc(targ, sizeof(FILETIME));       \
    memcpy(targ, list->items[x]->data, list->items[x]->size); \
    LE32_CPU(targ->dwLowDateTime);                            \
    LE32_CPU(targ->dwHighDateTime);                           \
}
// malloc space and copy the current item's data and size
#define LIST_COPY_SIZE(targ, type, mysize) {        \
    mysize = list->items[x]->size;                  \
    if (mysize) {                                   \
        targ = type realloc(targ, mysize);          \
        memcpy(targ, list->items[x]->data, mysize); \
    }                                               \
    else {                                          \
        SAFE_FREE(targ);                            \
        targ = NULL;                                \
    }                                               \
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


int pst_process(pst_num_array *list , pst_item *item, pst_item_attach *attach) {
    int32_t x, t;
    int next = 0;
    pst_item_extra_field *ef;

    DEBUG_ENT("pst_process");
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
                    if (list->items[x]->extra) {
                        ef = (pst_item_extra_field*) xmalloc(sizeof(pst_item_extra_field));
                        memset(ef, 0, sizeof(pst_item_extra_field));
                        ef->field_name = (char*) xmalloc(strlen(list->items[x]->extra)+1);
                        strcpy(ef->field_name, list->items[x]->extra);
                        LIST_COPY(ef->value, (char*));
                        ef->next = item->extra_fields;
                        item->extra_fields = ef;
                        DEBUG_EMAIL(("\"%s\" = \"%s\"\n", ef->field_name, ef->value));
                    }
                    else {
                        DEBUG_EMAIL(("NULL extra field\n"));
                    }
                    break;
                case 0x0002: // PR_ALTERNATE_RECIPIENT_ALLOWED
                    // If set to true, the sender allows this email to be autoforwarded
                    DEBUG_EMAIL(("AutoForward allowed - "));
                    MALLOC_EMAIL(item);
                    if (*(int16_t*)list->items[x]->data) {
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
                    DEBUG_EMAIL(("%s [%i]\n", ((int)t==0?"Low":((int)t==1?"Normal":"High")), t));
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
                    else if (pst_strincmp("IPM.Task", item->ascii_type, 8) == 0)
                        item->type = PST_TYPE_TASK;
                    else
                        item->type = PST_TYPE_OTHER;

                    DEBUG_EMAIL(("%s\n", item->ascii_type));
                    break;
                case 0x0023: // PR_ORIGINATOR_DELIVERY_REPORT_REQUESTED
                    // set if the sender wants a delivery report from all recipients
                    DEBUG_EMAIL(("Global Delivery Report - "));
                    MALLOC_EMAIL(item);
                    if (*(int16_t*)list->items[x]->data) {
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
                    //  0 Normal
                    //  1 Urgent
                    DEBUG_EMAIL(("Priority - "));
                    MALLOC_EMAIL(item);
                    memcpy(&(item->email->priority), list->items[x]->data, sizeof(item->email->priority));
                    LE32_CPU(item->email->priority);
                    t = item->email->priority;
                    DEBUG_EMAIL(("%s [%i]\n", (t<0?"NonUrgent":(t==0?"Normal":"Urgent")), t));
                    break;
                case 0x0029: // PR_READ_RECEIPT_REQUESTED
                    DEBUG_EMAIL(("Read Receipt - "));
                    MALLOC_EMAIL(item);
                    if (*(int16_t*)list->items[x]->data) {
                        DEBUG_EMAIL(("True\n"));
                        item->email->read_receipt = 1;
                    } else {
                        DEBUG_EMAIL(("False\n"));
                        item->email->read_receipt = 0;
                    }
                    break;
                case 0x002B: // PR_RECIPIENT_REASSIGNMENT_PROHIBITED
                    DEBUG_EMAIL(("Reassignment Prohibited (Private) - "));
                    if (*(int16_t*)list->items[x]->data) {
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
                    DEBUG_EMAIL(("%s [%i]\n", ((int)t==0?"None":((int)t==1?"Personal":
                                        ((int)t==2?"Private":"Company Confidential"))), t));
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
                    DEBUG_EMAIL(("%s [%i]\n", ((int)t==0?"None":((int)t==1?"Personal":
                                        ((int)t==2?"Private":"Company Confidential"))), t));
                    break;
                case 0x0037: // PR_SUBJECT raw subject
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
                            item->email->subject->off1 = (int)(unsigned)list->items[x]->data[0];
                            item->email->subject->off2 = (int)(unsigned)list->items[x]->data[1];
                            item->email->subject->subj = realloc(item->email->subject->subj, list->items[x]->size-1);
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
                    LIST_COPY_TIME(item->email->sent_date);
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
                    MALLOC_EMAIL(item);
                    LIST_COPY(item->email->outlook_recipient_name, (char*));
                    DEBUG_EMAIL(("%s\n", item->email->outlook_recipient_name));
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
                    if (*(int16_t*)list->items[x]->data) {
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
                    if (*(int16_t*)list->items[x]->data) {
                        DEBUG_EMAIL(("True\n"));
                        item->email->message_cc_me = 1;
                    } else {
                        DEBUG_EMAIL(("False\n"));
                        item->email->message_cc_me = 0;
                    }
                    break;
                case 0x0059: // PR_MESSAGE_RECIP_ME
                    // this user appears in TO, CC or BCC address list
                    DEBUG_EMAIL(("Message addressed to me - "));
                    MALLOC_EMAIL(item);
                    if (*(int16_t*)list->items[x]->data) {
                        DEBUG_EMAIL(("True\n"));
                        item->email->message_recip_me = 1;
                    } else {
                        DEBUG_EMAIL(("False\n"));
                        item->email->message_recip_me = 0;
                    }
                    break;
                case 0x0063: // PR_RESPONSE_REQUESTED
                    DEBUG_EMAIL(("Response requested - "));
                    if (*(int16_t*)list->items[x]->data) {
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
                case 0x0071: // PR_CONVERSATION_INDEX
                    DEBUG_EMAIL(("Conversation Index - "));
                    MALLOC_EMAIL(item);
                    memcpy(&(item->email->conv_index), list->items[x]->data, sizeof(item->email->conv_index));
                    DEBUG_EMAIL(("%i\n", item->email->conv_index));
                    break;
                case 0x0072: // PR_ORIGINAL_DISPLAY_BCC
                    DEBUG_EMAIL(("Original display bcc - "));
                    MALLOC_EMAIL(item);
                    LIST_COPY(item->email->original_bcc, (char*));
                    DEBUG_EMAIL(("%s\n", item->email->original_bcc));
                    break;
                case 0x0073: // PR_ORIGINAL_DISPLAY_CC
                    DEBUG_EMAIL(("Original display cc - "));
                    MALLOC_EMAIL(item);
                    LIST_COPY(item->email->original_cc, (char*));
                    DEBUG_EMAIL(("%s\n", item->email->original_cc));
                    break;
                case 0x0074: // PR_ORIGINAL_DISPLAY_TO
                    DEBUG_EMAIL(("Original display to - "));
                    MALLOC_EMAIL(item);
                    LIST_COPY(item->email->original_to, (char*));
                    DEBUG_EMAIL(("%s\n", item->email->original_to));
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
                    DEBUG_EMAIL(("%s\n", item->email->header));
                    break;
                case 0x0C17: // PR_REPLY_REQUESTED
                    DEBUG_EMAIL(("Reply Requested - "));
                    MALLOC_EMAIL(item);
                    if (*(int16_t*)list->items[x]->data) {
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
                    if (*(int16_t*)list->items[x]->data) {
                        DEBUG_EMAIL(("True\n"));
                        item->email->delete_after_submit = 1;
                    } else {
                        DEBUG_EMAIL(("False\n"));
                        item->email->delete_after_submit = 0;
                    }
                    break;
                case 0x0E02: // PR_DISPLAY_BCC BCC Addresses
                    DEBUG_EMAIL(("Display BCC Addresses - "));
                    MALLOC_EMAIL(item);
                    LIST_COPY(item->email->bcc_address, (char*));
                    DEBUG_EMAIL(("%s\n", item->email->bcc_address));
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
                    LIST_COPY_TIME(item->email->arrival_date);
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
                    //   cannot update to the rtf
                    DEBUG_EMAIL(("Compressed RTF in Sync - "));
                    MALLOC_EMAIL(item);
                    if (*(int16_t*)list->items[x]->data) {
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
                    t = (*(int32_t*)list->items[x]->data);
                    LE32_CPU(t);
                    attach->size = (size_t)t;
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
                    // rtf data is lzw compressed
                    DEBUG_EMAIL(("RTF Compressed body - "));
                    MALLOC_EMAIL(item);
                    LIST_COPY_SIZE(item->email->rtf_compressed, (char*), item->email->rtf_compressed_size);
                    //DEBUG_EMAIL_HEXPRINT((char*)item->email->rtf_compressed, item->email->rtf_compressed_size);
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
                    //  DEBUG_EMAIL(("%s\n", item->email->htmlbody));
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
                    LIST_COPY_TIME(item->create_date);
                    DEBUG_EMAIL(("%s", fileTimeToAscii(item->create_date)));
                    break;
                case 0x3008: // PR_LAST_MODIFICATION_TIME Date 5 - Modify Date
                    DEBUG_EMAIL(("Date 5 (Modify Date) - "));
                    LIST_COPY_TIME(item->modify_date);
                    DEBUG_EMAIL(("%s", fileTimeToAscii(item->modify_date)));
                    break;
                case 0x300B: // PR_SEARCH_KEY Record Header 2
                    DEBUG_EMAIL(("Record Search 2 -- NOT HANDLED\n"));
                    break;
                case 0x35DF: // PR_VALID_FOLDER_MASK
                    // States which folders are valid for this message store
                    // FOLDER_IPM_SUBTREE_VALID 0x1
                    // FOLDER_IPM_INBOX_VALID   0x2
                    // FOLDER_IPM_OUTBOX_VALID  0x4
                    // FOLDER_IPM_WASTEBOX_VALID 0x8
                    // FOLDER_IPM_SENTMAIL_VALID 0x10
                    // FOLDER_VIEWS_VALID        0x20
                    // FOLDER_COMMON_VIEWS_VALID 0x40
                    // FOLDER_FINDER_VALID       0x80
                    DEBUG_EMAIL(("Valid Folder Mask - "));
                    MALLOC_MESSAGESTORE(item);
                    memcpy(&(item->message_store->valid_mask), list->items[x]->data, sizeof(item->message_store->valid_mask));
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
                case 0x35E2: // PR_IPM_OUTBOX_ENTRYID
                    DEBUG_EMAIL(("Default Outbox Folder record - "));
                    MALLOC_MESSAGESTORE(item);
                    LIST_COPY(item->message_store->default_outbox_folder, (pst_entryid*));
                    LE32_CPU(item->message_store->default_outbox_folder->id);
                    DEBUG_EMAIL(("[id = %#x]\n", item->message_store->default_outbox_folder->id));
                    break;
                case 0x35E3: // PR_IPM_WASTEBASKET_ENTRYID
                    DEBUG_EMAIL(("Deleted Items Folder record - "));
                    MALLOC_MESSAGESTORE(item);
                    LIST_COPY(item->message_store->deleted_items_folder, (pst_entryid*));
                    LE32_CPU(item->message_store->deleted_items_folder->id);
                    DEBUG_EMAIL(("[id = %#x]\n", item->message_store->deleted_items_folder->id));
                    break;
                case 0x35E4: // PR_IPM_SENTMAIL_ENTRYID
                    DEBUG_EMAIL(("Sent Items Folder record - "));
                    MALLOC_MESSAGESTORE(item);
                    LIST_COPY(item->message_store->sent_items_folder, (pst_entryid*));
                    LE32_CPU(item->message_store->sent_items_folder->id);
                    DEBUG_EMAIL(("[id = %#x]\n", item->message_store->sent_items_folder->id));
                    break;
                case 0x35E5: // PR_VIEWS_ENTRYID
                    DEBUG_EMAIL(("User Views Folder record - "));
                    MALLOC_MESSAGESTORE(item);
                    LIST_COPY(item->message_store->user_views_folder, (pst_entryid*));
                    LE32_CPU(item->message_store->user_views_folder->id);
                    DEBUG_EMAIL(("[id = %#x]\n", item->message_store->user_views_folder->id));
                    break;
                case 0x35E6: // PR_COMMON_VIEWS_ENTRYID
                    DEBUG_EMAIL(("Common View Folder record - "));
                    MALLOC_MESSAGESTORE(item);
                    LIST_COPY(item->message_store->common_view_folder, (pst_entryid*));
                    LE32_CPU(item->message_store->common_view_folder->id);
                    DEBUG_EMAIL(("[id = %#x]\n", item->message_store->common_view_folder->id));
                    break;
                case 0x35E7: // PR_FINDER_ENTRYID
                    DEBUG_EMAIL(("Search Root Folder record - "));
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
                    if (*(int16_t*)list->items[x]->data) {
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
                        DEBUG_EMAIL(("Seen a Reference. The data hasn't been loaded yet. [%#"PRIx64"][%#x]\n",
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
                    DEBUG_EMAIL(("Attachment method - "));
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
                    if (*(int16_t*)list->items[x]->data) {
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
                    if (*(int16_t*)list->items[x]->data) {
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
                    if (*(int16_t*)list->items[x]->data) {
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
                    LIST_COPY_TIME(item->contact->wedding_anniversary);
                    DEBUG_EMAIL(("%s\n", fileTimeToAscii(item->contact->wedding_anniversary)));
                    break;
                case 0x3A42: // PR_BIRTHDAY
                    DEBUG_EMAIL(("Birthday - "));
                    MALLOC_CONTACT(item);
                    LIST_COPY_TIME(item->contact->birthday);
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
                    memcpy(&item->contact->gender, list->items[x]->data, sizeof(item->contact->gender));
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
                    if (attach) {
                        uint32_t tempid;
                        MOVE_NEXT(attach);
                        memcpy(&(tempid), list->items[x]->data, sizeof(tempid));
                        LE32_CPU(tempid);
                        attach->id2_val = tempid;
                        DEBUG_EMAIL(("%#"PRIx64"\n", attach->id2_val));
                    } else {
                        DEBUG_EMAIL(("NOT AN ATTACHMENT: %#x\n", list->items[x]->id));
                    }
                    break;
                case 0x67FF: // Extra Property Identifier (Password CheckSum)
                    DEBUG_EMAIL(("Password checksum [0x67FF] - "));
                    MALLOC_MESSAGESTORE(item);
                    memcpy(&(item->message_store->pwd_chksum), list->items[x]->data, sizeof(item->message_store->pwd_chksum));
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
                    DEBUG_EMAIL(("u1 %#x id %#x\n", item->message_store->top_of_folder->u1, item->message_store->top_of_folder->id));
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
                case 0x8045: // Work address street
                    DEBUG_EMAIL(("Work address street - "));
                    MALLOC_CONTACT(item);
                    LIST_COPY(item->contact->work_address_street, (char*));
                    DEBUG_EMAIL(("%s\n", item->contact->work_address_street));
                    break;
                case 0x8046: // Work address city
                    DEBUG_EMAIL(("Work address city - "));
                    MALLOC_CONTACT(item);
                    LIST_COPY(item->contact->work_address_city, (char*));
                    DEBUG_EMAIL(("%s\n", item->contact->work_address_city));
                    break;
                case 0x8047: // Work address state
                    DEBUG_EMAIL(("Work address state - "));
                    MALLOC_CONTACT(item);
                    LIST_COPY(item->contact->work_address_state, (char*));
                    DEBUG_EMAIL(("%s\n", item->contact->work_address_state));
                    break;
                case 0x8048: // Work address postalcode
                    DEBUG_EMAIL(("Work address postalcode - "));
                    MALLOC_CONTACT(item);
                    LIST_COPY(item->contact->work_address_postalcode, (char*));
                    DEBUG_EMAIL(("%s\n", item->contact->work_address_postalcode));
                    break;
                case 0x8049: // Work address country
                    DEBUG_EMAIL(("Work address country - "));
                    MALLOC_CONTACT(item);
                    LIST_COPY(item->contact->work_address_country, (char*));
                    DEBUG_EMAIL(("%s\n", item->contact->work_address_country));
                    break;
                case 0x804A: // Work address postofficebox
                    DEBUG_EMAIL(("Work address postofficebox - "));
                    MALLOC_CONTACT(item);
                    LIST_COPY(item->contact->work_address_postofficebox, (char*));
                    DEBUG_EMAIL(("%s\n", item->contact->work_address_postofficebox));
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
                case 0x820d: // Appointment start
                    DEBUG_EMAIL(("Appointment Date Start - "));
                    MALLOC_APPOINTMENT(item);
                    LIST_COPY_TIME(item->appointment->start);
                    DEBUG_EMAIL(("%s\n", fileTimeToAscii(item->appointment->start)));
                    break;
                case 0x820e: // Appointment end
                    DEBUG_EMAIL(("Appointment Date End - "));
                    MALLOC_APPOINTMENT(item);
                    LIST_COPY_TIME(item->appointment->end);
                    DEBUG_EMAIL(("%s\n", fileTimeToAscii(item->appointment->end)));
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
                    if (*(int16_t*)list->items[x]->data) {
                        DEBUG_EMAIL(("True\n"));
                        item->appointment->all_day = 1;
                    } else {
                        DEBUG_EMAIL(("False\n"));
                        item->appointment->all_day = 0;
                    }
                    break;
                case 0x8231: // Recurrence type
                    // 1: Daily
                    // 2: Weekly
                    // 3: Monthly
                    // 4: Yearly
                    DEBUG_EMAIL(("Appointment reccurs - "));
                    MALLOC_APPOINTMENT(item);
                    memcpy(&(item->appointment->recurrence_type), list->items[x]->data, sizeof(item->appointment->recurrence_type));
                    LE32_CPU(item->appointment->recurrence_type);
                    switch (item->appointment->recurrence_type) {
                        case PST_APP_RECUR_DAILY:
                            DEBUG_EMAIL(("Daily\n")); break;
                        case PST_APP_RECUR_WEEKLY:
                            DEBUG_EMAIL(("Weekly\n")); break;
                        case PST_APP_RECUR_MONTHLY:
                            DEBUG_EMAIL(("Monthly\n")); break;
                        case PST_APP_RECUR_YEARLY:
                            DEBUG_EMAIL(("Yearly\n")); break;
                        default:
                            DEBUG_EMAIL(("Unknown Value: %d\n", item->appointment->recurrence_type)); break;
                    }
                    break;
                case 0x8232: // Recurrence description
                    DEBUG_EMAIL(("Appointment recurrence description - "));
                    MALLOC_APPOINTMENT(item);
                    LIST_COPY(item->appointment->recurrence, (char*));
                    DEBUG_EMAIL(("%s\n", item->appointment->recurrence));
                    break;
                case 0x8234: // TimeZone as String
                    DEBUG_EMAIL(("TimeZone of times - "));
                    MALLOC_APPOINTMENT(item);
                    LIST_COPY(item->appointment->timezonestring, (char*));
                    DEBUG_EMAIL(("%s\n", item->appointment->timezonestring));
                    break;
                case 0x8235: // Recurrence start date
                    DEBUG_EMAIL(("Recurrence Start Date - "));
                    MALLOC_APPOINTMENT(item);
                    LIST_COPY_TIME(item->appointment->recurrence_start);
                    DEBUG_EMAIL(("%s\n", fileTimeToAscii(item->appointment->recurrence_start)));
                    break;
                case 0x8236: // Recurrence end date
                    DEBUG_EMAIL(("Recurrence End Date - "));
                    MALLOC_APPOINTMENT(item);
                    LIST_COPY_TIME(item->appointment->recurrence_end);
                    DEBUG_EMAIL(("%s\n", fileTimeToAscii(item->appointment->recurrence_end)));
                    break;
                case 0x8501: // Reminder minutes before appointment start
                    DEBUG_EMAIL(("Alarm minutes - "));
                    MALLOC_APPOINTMENT(item);
                    memcpy(&(item->appointment->alarm_minutes), list->items[x]->data, sizeof(item->appointment->alarm_minutes));
                    LE32_CPU(item->appointment->alarm_minutes);
                    DEBUG_EMAIL(("%i\n", item->appointment->alarm_minutes));
                    break;
                case 0x8503: // Reminder alarm
                    DEBUG_EMAIL(("Reminder alarm - "));
                    MALLOC_APPOINTMENT(item);
                    if (*(int16_t*)list->items[x]->data) {
                        DEBUG_EMAIL(("True\n"));
                        item->appointment->alarm = 1;
                    } else {
                        DEBUG_EMAIL(("False\n"));
                        item->appointment->alarm = 0;
                    }
                    break;
                case 0x8516: // Common start
                    DEBUG_EMAIL(("Common Start Date - "));
                    DEBUG_EMAIL(("%s\n", fileTimeToAscii((FILETIME*)list->items[x]->data)));
                    break;
                case 0x8517: // Common end
                    DEBUG_EMAIL(("Common End Date - "));
                    DEBUG_EMAIL(("%s\n", fileTimeToAscii((FILETIME*)list->items[x]->data)));
                    break;
                case 0x851f: // Play reminder sound filename
                    DEBUG_EMAIL(("Appointment reminder sound filename - "));
                    MALLOC_APPOINTMENT(item);
                    LIST_COPY(item->appointment->alarm_filename, (char*));
                    DEBUG_EMAIL(("%s\n", item->appointment->alarm_filename));
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
                    LIST_COPY_TIME(item->appointment->reminder);
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
                    LIST_COPY_TIME(item->journal->start);
                    DEBUG_EMAIL(("%s\n", fileTimeToAscii(item->journal->start)));
                    break;
                case 0x8708: // Journal End date/time
                    DEBUG_EMAIL(("End Timestamp - "));
                    MALLOC_JOURNAL(item);
                    LIST_COPY_TIME(item->journal->end);
                    DEBUG_EMAIL(("%s\n", fileTimeToAscii(item->journal->end)));
                    break;
                case 0x8712: // Title?
                    DEBUG_EMAIL(("Journal Entry Type - "));
                    MALLOC_JOURNAL(item);
                    LIST_COPY(item->journal->type, (char*));
                    DEBUG_EMAIL(("%s\n", item->journal->type));
                    break;
                default:
                    if (list->items[x]->type == (uint32_t)0x0002) {
                        DEBUG_EMAIL(("Unknown type %#x 16bit int = %hi\n", list->items[x]->id,
                            *(int16_t*)list->items[x]->data));

                    } else if (list->items[x]->type == (uint32_t)0x0003) {
                        DEBUG_EMAIL(("Unknown type %#x 32bit int = %i\n", list->items[x]->id,
                            *(int32_t*)list->items[x]->data));

                    } else if (list->items[x]->type == (uint32_t)0x0004) {
                        DEBUG_EMAIL(("Unknown type %#x 4-byte floating [size = %#x]\n", list->items[x]->id,
                            list->items[x]->size));
                        DEBUG_HEXDUMP(list->items[x]->data, list->items[x]->size);

                    } else if (list->items[x]->type == (uint32_t)0x0005) {
                        DEBUG_EMAIL(("Unknown type %#x double floating [size = %#x]\n", list->items[x]->id,
                            list->items[x]->size));
                        DEBUG_HEXDUMP(list->items[x]->data, list->items[x]->size);

                    } else if (list->items[x]->type == (uint32_t)0x0006) {
                        DEBUG_EMAIL(("Unknown type %#x signed 64bit int = %lli\n", list->items[x]->id,
                            *(int64_t*)list->items[x]->data));
                        DEBUG_HEXDUMP(list->items[x]->data, list->items[x]->size);

                    } else if (list->items[x]->type == (uint32_t)0x0007) {
                        DEBUG_EMAIL(("Unknown type %#x application time [size = %#x]\n", list->items[x]->id,
                            list->items[x]->size));
                        DEBUG_HEXDUMP(list->items[x]->data, list->items[x]->size);

                    } else if (list->items[x]->type == (uint32_t)0x000a) {
                        DEBUG_EMAIL(("Unknown type %#x 32bit error value = %i\n", list->items[x]->id,
                            *(int32_t*)list->items[x]->data));

                    } else if (list->items[x]->type == (uint32_t)0x000b) {
                        DEBUG_EMAIL(("Unknown type %#x 16bit boolean = %s [%hi]\n", list->items[x]->id,
                            (*((int16_t*)list->items[x]->data)!=0?"True":"False"),
                            *((int16_t*)list->items[x]->data)));

                    } else if (list->items[x]->type == (uint32_t)0x000d) {
                        DEBUG_EMAIL(("Unknown type %#x Embedded object [size = %#x]\n", list->items[x]->id,
                            list->items[x]->size));
                        DEBUG_HEXDUMP(list->items[x]->data, list->items[x]->size);

                    } else if (list->items[x]->type == (uint32_t)0x0014) {
                        DEBUG_EMAIL(("Unknown type %#x signed 64bit int = %lli\n", list->items[x]->id,
                            *(int64_t*)list->items[x]->data));
                        DEBUG_HEXDUMP(list->items[x]->data, list->items[x]->size);

                    } else if (list->items[x]->type == (uint32_t)0x001e) {
                        DEBUG_EMAIL(("Unknown type %#x String Data = \"%s\"\n", list->items[x]->id,
                            list->items[x]->data));

                    } else if (list->items[x]->type == (uint32_t)0x001f) {
                        DEBUG_EMAIL(("Unknown type %#x Unicode String Data [size = %#x]\n", list->items[x]->id,
                            list->items[x]->size));
                        DEBUG_HEXDUMP(list->items[x]->data, list->items[x]->size);

                    } else if (list->items[x]->type == (uint32_t)0x0040) {
                        DEBUG_EMAIL(("Unknown type %#x Date = \"%s\"\n", list->items[x]->id,
                            fileTimeToAscii((FILETIME*)list->items[x]->data)));

                    } else if (list->items[x]->type == (uint32_t)0x0048) {
                        DEBUG_EMAIL(("Unknown type %#x OLE GUID [size = %#x]\n", list->items[x]->id,
                            list->items[x]->size));
                        DEBUG_HEXDUMP(list->items[x]->data, list->items[x]->size);

                    } else if (list->items[x]->type == (uint32_t)0x0102) {
                        DEBUG_EMAIL(("Unknown type %#x Binary Data [size = %#x]\n", list->items[x]->id,
                            list->items[x]->size));
                        DEBUG_HEXDUMP(list->items[x]->data, list->items[x]->size);

                    } else if (list->items[x]->type == (uint32_t)0x1003) {
                        DEBUG_EMAIL(("Unknown type %#x Array of 32 bit values [size = %#x]\n", list->items[x]->id,
                            list->items[x]->size));
                        DEBUG_HEXDUMP(list->items[x]->data, list->items[x]->size);

                    } else if (list->items[x]->type == (uint32_t)0x1014) {
                        DEBUG_EMAIL(("Unknown type %#x Array of 64 bit values [siize = %#x]\n", list->items[x]->id,
                            list->items[x]->size));
                        DEBUG_HEXDUMP(list->items[x]->data, list->items[x]->size);

                    } else if (list->items[x]->type == (uint32_t)0x101E) {
                        DEBUG_EMAIL(("Unknown type %#x Array of Strings [size = %#x]\n", list->items[x]->id,
                            list->items[x]->size));
                        DEBUG_HEXDUMP(list->items[x]->data, list->items[x]->size);

                    } else if (list->items[x]->type == (uint32_t)0x101F) {
                        DEBUG_EMAIL(("Unknown type %#x Array of Unicode Strings [size = %#x]\n", list->items[x]->id,
                            list->items[x]->size));
                        DEBUG_HEXDUMP(list->items[x]->data, list->items[x]->size);

                    } else if (list->items[x]->type == (uint32_t)0x1102) {
                        DEBUG_EMAIL(("Unknown type %#x Array of binary data blobs [size = %#x]\n", list->items[x]->id,
                            list->items[x]->size));
                        DEBUG_HEXDUMP(list->items[x]->data, list->items[x]->size);

                    } else {
                        DEBUG_EMAIL(("Unknown type %#x Not Printable [%#x]\n", list->items[x]->id,
                            list->items[x]->type));
                        DEBUG_HEXDUMP(list->items[x]->data, list->items[x]->size);
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


void pst_free_list(pst_num_array *list) {
    pst_num_array *l;
    DEBUG_ENT("pst_free_list");
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
}


void pst_free_id2(pst_index2_ll * head) {
    pst_index2_ll *t;
    DEBUG_ENT("pst_free_id2");
    while (head) {
        t = head->next;
        free (head);
        head = t;
    }
    DEBUG_RET();
}


void pst_free_id (pst_index_ll *head) {
    pst_index_ll *t;
    DEBUG_ENT("pst_free_id");
    while (head) {
        t = head->next;
        free(head);
        head = t;
    }
    DEBUG_RET();
}


void pst_free_desc (pst_desc_ll *head) {
    pst_desc_ll *t;
    DEBUG_ENT("pst_free_desc");
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
        else      DIE(("head is NULL"));

        head = t;
    }
    DEBUG_RET();
}


void pst_free_xattrib(pst_x_attrib_ll *x) {
    pst_x_attrib_ll *t;
    DEBUG_ENT("pst_free_xattrib");
    while (x) {
        if (x->data) free(x->data);
        t = x->next;
        free(x);
        x = t;
    }
    DEBUG_RET();
}


pst_index2_ll * pst_build_id2(pst_file *pf, pst_index_ll* list, pst_index2_ll* head_ptr) {
    pst_block_header block_head;
    pst_index2_ll *head = NULL, *tail = NULL;
    uint16_t x = 0;
    char *b_ptr = NULL;
    char *buf = NULL;
    pst_id2_assoc id2_rec;
    pst_index_ll *i_ptr = NULL;
    pst_index2_ll *i2_ptr = NULL;
    DEBUG_ENT("pst_build_id2");

    if (head_ptr) {
        head = head_ptr;
        while (head_ptr) head_ptr = (tail = head_ptr)->next;
    }
    if (pst_read_block_size(pf, list->offset, list->size, &buf) < list->size) {
        //an error occured in block read
        WARN(("block read error occured. offset = %#"PRIx64", size = %#"PRIx64"\n", list->offset, list->size));
        if (buf) free(buf);
        DEBUG_RET();
        return NULL;
    }
    DEBUG_HEXDUMPC(buf, list->size, 16);

    memcpy(&block_head, buf, sizeof(block_head));
    LE16_CPU(block_head.type);
    LE16_CPU(block_head.count);

    if (block_head.type != (uint16_t)0x0002) { // some sort of constant?
        WARN(("Unknown constant [%#hx] at start of id2 values [offset %#"PRIx64"].\n", block_head.type, list->offset));
        if (buf) free(buf);
        DEBUG_RET();
        return NULL;
    }

    DEBUG_INDEX(("ID %#"PRIx64" is likely to be a description record. Count is %i (offset %#"PRIx64")\n",
            list->id, block_head.count, list->offset));
    x = 0;
    b_ptr = buf + ((pf->do_read64) ? 0x08 : 0x04);
    while (x < block_head.count) {
        b_ptr += pst_decode_assoc(pf, &id2_rec, b_ptr);
        DEBUG_INDEX(("\tid2 = %#x, id = %#"PRIx64", table2 = %#"PRIx64"\n", id2_rec.id2, id2_rec.id, id2_rec.table2));
        if ((i_ptr = pst_getID(pf, id2_rec.id)) == NULL) {
            DEBUG_WARN(("\t\t%#"PRIx64" - Not Found\n", id2_rec.id));
        } else {
            DEBUG_INDEX(("\t\t%#"PRIx64" - Offset %#"PRIx64", u1 %#"PRIx64", Size %lli(%#"PRIx64")\n", i_ptr->id, i_ptr->offset, i_ptr->u1, i_ptr->size, i_ptr->size));
            // add it to the linked list
            i2_ptr = (pst_index2_ll*) xmalloc(sizeof(pst_index2_ll));
            i2_ptr->id2  = id2_rec.id2;
            i2_ptr->id   = i_ptr;
            i2_ptr->next = NULL;
            if (!head) head = i2_ptr;
            if (tail)  tail->next = i2_ptr;
            tail = i2_ptr;
            if (id2_rec.table2 != 0) {
                if ((i_ptr = pst_getID(pf, id2_rec.table2)) == NULL) {
                    DEBUG_WARN(("\tTable2 [%#x] not found\n", id2_rec.table2));
                }
                else {
                    DEBUG_INDEX(("\tGoing deeper for table2 [%#x]\n", id2_rec.table2));
                    if ((i2_ptr = pst_build_id2(pf, i_ptr, head))) {
                    //  DEBUG_INDEX(("pst_build_id2(): \t\tAdding new list onto end of current\n"));
                    //  if (!head)
                    //    head = i2_ptr;
                    //  if (tail)
                    //    tail->next = i2_ptr;
                    //  while (i2_ptr->next)
                    //    i2_ptr = i2_ptr->next;
                    //    tail = i2_ptr;
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


void pst_freeItem(pst_item *item) {
    pst_item_attach *t;
    pst_item_extra_field *et;

    DEBUG_ENT("pst_freeItem");
    if (item) {
        if (item->email) {
            SAFE_FREE(item->email->arrival_date);
            SAFE_FREE(item->email->body);
            SAFE_FREE(item->email->cc_address);
            SAFE_FREE(item->email->bcc_address);
            SAFE_FREE(item->email->common_name);
            SAFE_FREE(item->email->encrypted_body);
            SAFE_FREE(item->email->encrypted_htmlbody);
            SAFE_FREE(item->email->header);
            SAFE_FREE(item->email->htmlbody);
            SAFE_FREE(item->email->in_reply_to);
            SAFE_FREE(item->email->messageid);
            SAFE_FREE(item->email->original_bcc);
            SAFE_FREE(item->email->original_cc);
            SAFE_FREE(item->email->original_to);
            SAFE_FREE(item->email->outlook_recipient);
            SAFE_FREE(item->email->outlook_recipient_name);
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
            SAFE_FREE(item->message_store->top_of_personal_folder);
            SAFE_FREE(item->message_store->default_outbox_folder);
            SAFE_FREE(item->message_store->deleted_items_folder);
            SAFE_FREE(item->message_store->sent_items_folder);
            SAFE_FREE(item->message_store->user_views_folder);
            SAFE_FREE(item->message_store->common_view_folder);
            SAFE_FREE(item->message_store->search_root_folder);
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
            SAFE_FREE(item->contact->work_address_street);
            SAFE_FREE(item->contact->work_address_city);
            SAFE_FREE(item->contact->work_address_state);
            SAFE_FREE(item->contact->work_address_postalcode);
            SAFE_FREE(item->contact->work_address_country);
            SAFE_FREE(item->contact->work_address_postofficebox);
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
            SAFE_FREE(item->appointment->alarm_filename);
            SAFE_FREE(item->appointment->start);
            SAFE_FREE(item->appointment->end);
            SAFE_FREE(item->appointment->timezonestring);
            SAFE_FREE(item->appointment->recurrence);
            SAFE_FREE(item->appointment->recurrence_start);
            SAFE_FREE(item->appointment->recurrence_end);
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
  * Otherwise, the high order 16 bits of offset is the index into the subblocks, and
  * the (low order 16 bits of offset)>>4 is an index into the table of offsets in the subblock.
*/
int pst_getBlockOffsetPointer(pst_file *pf, pst_index2_ll *i2_head, pst_subblocks *subblocks, uint32_t offset, pst_block_offset_pointer *p) {
    size_t size;
    pst_block_offset block_offset;
    DEBUG_ENT("pst_getBlockOffsetPointer");
    if (p->needfree) free(p->from);
    p->from     = NULL;
    p->to       = NULL;
    p->needfree = 0;
    if (!offset) {
        // no data
        p->from = p->to = NULL;
    }
    else if ((offset & 0xf) == (uint32_t)0xf) {
        // external index reference
        DEBUG_WARN(("Found id2 %#x value. Will follow it\n", offset));
        size = pst_ff_getID2block(pf, offset, i2_head, &(p->from));
        if (size) {
            p->to = p->from + size;
            p->needfree = 1;
        }
        else {
            if (p->from) {
                DEBUG_WARN(("size zero but non-null pointer\n"));
                free(p->from);
            }
            p->from = p->to = NULL;
        }
    }
    else {
        // internal index reference
        size_t subindex  = offset >> 16;
        size_t suboffset = offset & 0xffff;
        if (subindex < subblocks->subblock_count) {
            if (pst_getBlockOffset(subblocks->subs[subindex].buf,
                                   subblocks->subs[subindex].read_size,
                                   subblocks->subs[subindex].i_offset,
                                   suboffset, &block_offset)) {
                p->from = subblocks->subs[subindex].buf + block_offset.from;
                p->to   = subblocks->subs[subindex].buf + block_offset.to;
            }
        }
    }
    DEBUG_RET();
    return (p->from) ? 0 : 1;
}


int pst_getBlockOffset(char *buf, size_t read_size, uint32_t i_offset, uint32_t offset, pst_block_offset *p) {
    uint32_t low = offset & 0xf;
    uint32_t of1 = offset >> 4;
    DEBUG_ENT("pst_getBlockOffset");
    if (!p || !buf || !i_offset || low || (i_offset+2+of1+sizeof(*p) > read_size)) {
        DEBUG_WARN(("p is NULL or buf is NULL or offset is 0 or offset has low bits or beyond read size (%p, %p, %#x, %i, %i)\n", p, buf, offset, read_size, i_offset));
        DEBUG_RET();
        return 0;
    }
    memcpy(&(p->from), &(buf[(i_offset+2)+of1]), sizeof(p->from));
    memcpy(&(p->to), &(buf[(i_offset+2)+of1+sizeof(p->from)]), sizeof(p->to));
    LE16_CPU(p->from);
    LE16_CPU(p->to);
    DEBUG_WARN(("get block offset finds from=%i(%#x), to=%i(%#x)\n", p->from, p->from, p->to, p->to));
    if (p->from > p->to) {
        DEBUG_WARN(("get block offset from > to"));
        DEBUG_RET();
        return 0;
    }
    DEBUG_RET();
    return 1;
}


pst_index_ll* pst_getID(pst_file* pf, uint64_t id) {
    pst_index_ll *ptr;
    DEBUG_ENT("pst_getID");
    if (id == 0) {
        DEBUG_RET();
        return NULL;
    }

    //if (id & 1) DEBUG_INDEX(("have odd id bit %#"PRIx64"\n", id));
    //if (id & 2) DEBUG_INDEX(("have two id bit %#"PRIx64"\n", id));
    id -= (id & 1);

    DEBUG_INDEX(("Trying to find %#"PRIx64"\n", id));
    ptr = pf->i_head;
    while (ptr && (ptr->id != id)) {
        ptr = ptr->next;
    }
    if (ptr) {DEBUG_INDEX(("Found Value %#"PRIx64"\n", id));            }
    else     {DEBUG_INDEX(("ERROR: Value %#"PRIx64" not found\n", id)); }
    DEBUG_RET();
    return ptr;
}


pst_index_ll * pst_getID2(pst_index2_ll *ptr, uint64_t id) {
    DEBUG_ENT("pst_getID2");
    DEBUG_INDEX(("Head = %p id = %#"PRIx64"\n", ptr, id));
    while (ptr && (ptr->id2 != id)) {
        ptr = ptr->next;
    }
    if (ptr) {
        if (ptr->id) {DEBUG_INDEX(("Found value %#"PRIx64"\n", ptr->id->id));   }
        else         {DEBUG_INDEX(("Found value, though it is NULL!\n"));}
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
 * @param pf    global pst file pointer
 * @param id    the id we are looking for
 *
 * @return pointer to the pst_desc_ll node in the descriptor tree
*/
pst_desc_ll* pst_getDptr(pst_file *pf, uint64_t id) {
    pst_desc_ll *ptr = pf->d_head;
    DEBUG_ENT("pst_getDptr");
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


void pst_printDptr(pst_file *pf, pst_desc_ll *ptr) {
    DEBUG_ENT("pst_printDptr");
    while (ptr) {
        DEBUG_INDEX(("%#x [%i] desc=%#x, list=%#x\n", ptr->id, ptr->no_child,
                    (ptr->desc==NULL?0:ptr->desc->id),
                    (ptr->list_index==NULL?0:ptr->list_index->id)));
        if (ptr->child) {
            pst_printDptr(pf, ptr->child);
        }
        ptr = ptr->next;
    }
    DEBUG_RET();
}


void pst_printIDptr(pst_file* pf) {
    pst_index_ll *ptr = pf->i_head;
    DEBUG_ENT("pst_printIDptr");
    while (ptr) {
        DEBUG_INDEX(("%#x offset=%#x size=%#x\n", ptr->id, ptr->offset, ptr->size));
        ptr = ptr->next;
    }
    DEBUG_RET();
}


void pst_printID2ptr(pst_index2_ll *ptr) {
    DEBUG_ENT("pst_printID2ptr");
    while (ptr) {
        DEBUG_INDEX(("%#x id=%#x\n", ptr->id2, (ptr->id!=NULL?ptr->id->id:0)));
        ptr = ptr->next;
    }
    DEBUG_RET();
}


/**
 * Read a block of data from file into memory
 * @param pf     PST file
 * @param offset offset in the pst file of the data
 * @param size   size of the block to be read
 * @param buf    reference to pointer to buffer. If this pointer
                 is non-NULL, it will first be free()d
 * @return       size of block read into memory
 */
size_t pst_read_block_size(pst_file *pf, off_t offset, size_t size, char **buf) {
    size_t rsize;
    DEBUG_ENT("pst_read_block_size");
    DEBUG_READ(("Reading block from %#"PRIx64", %x bytes\n", offset, size));

    if (*buf) {
        DEBUG_READ(("Freeing old memory\n"));
        free(*buf);
    }
    *buf = (char*) xmalloc(size);

    rsize = pst_getAtPos(pf, offset, *buf, size);
    if (rsize != size) {
        DEBUG_WARN(("Didn't read all the data. fread returned less [%i instead of %i]\n", rsize, size));
        if (feof(pf->fp)) {
            DEBUG_WARN(("We tried to read past the end of the file at [offset %#"PRIx64", size %#x]\n", offset, size));
        } else if (ferror(pf->fp)) {
            DEBUG_WARN(("Error is set on file stream.\n"));
        } else {
            DEBUG_WARN(("I can't tell why it failed\n"));
        }
    }

    DEBUG_RET();
    return rsize;
}


int pst_decrypt(uint64_t id, char *buf, size_t size, unsigned char type) {
    size_t x = 0;
    unsigned char y;
    DEBUG_ENT("pst_decrypt");
    if (!buf) {
        DEBUG_RET();
        return -1;
    }

    if (type == PST_COMP_ENCRYPT) {
        x = 0;
        while (x < size) {
            y = (unsigned char)(buf[x]);
            buf[x] = (char)comp_enc[y]; // transpose from encrypt array
            x++;
        }

    } else if (type == PST_ENCRYPT) {
        // The following code was based on the information at
        // http://www.passcape.com/outlook_passwords.htm
        uint16_t salt = (uint16_t) (((id & 0x00000000ffff0000) >> 16) ^ (id & 0x000000000000ffff));
        x = 0;
        while (x < size) {
            uint8_t losalt = (salt & 0x00ff);
            uint8_t hisalt = (salt & 0xff00) >> 8;
            y = (unsigned char)buf[x];
            y += losalt;
            y = comp_high1[y];
            y += hisalt;
            y = comp_high2[y];
            y -= hisalt;
            y = comp_enc[y];
            y -= losalt;
            buf[x] = (char)y;
            x++;
            salt++;
        }

    } else {
        WARN(("Unknown encryption: %i. Cannot decrypt\n", type));
        DEBUG_RET();
        return -1;
    }
    DEBUG_RET();
    return 0;
}


uint64_t pst_getIntAt(pst_file *pf, char *buf) {
    uint64_t buf64;
    uint32_t buf32;
    if (pf->do_read64) {
        memcpy(&buf64, buf, sizeof(buf64));
        LE64_CPU(buf64);
        return buf64;
    }
    else {
        memcpy(&buf32, buf, sizeof(buf32));
        LE32_CPU(buf32);
        return buf32;
    }
}


uint64_t pst_getIntAtPos(pst_file *pf, off_t pos ) {
    uint64_t buf64;
    uint32_t buf32;
    if (pf->do_read64) {
        (void)pst_getAtPos(pf, pos, &buf64, sizeof(buf64));
        LE64_CPU(buf64);
        return buf64;
    }
    else {
        (void)pst_getAtPos(pf, pos, &buf32, sizeof(buf32));
        LE32_CPU(buf32);
        return buf32;
    }
}

/**
 * Read part of the pst file.
 *
 * @param pf   PST file structure
 * @param pos  offset of the data in the pst file
 * @param buf  buffer to contain the data
 * @param size size of the buffer and the amount of data to be read
 * @return     actual read size, 0 if seek error
 */

size_t pst_getAtPos(pst_file *pf, off_t pos, void* buf, size_t size) {
    size_t rc;
    DEBUG_ENT("pst_getAtPos");
//  pst_block_recorder **t = &pf->block_head;
//  pst_block_recorder *p = pf->block_head;
//  while (p && ((p->offset+p->size) <= pos)) {
//      t = &p->next;
//      p = p->next;
//  }
//  if (p && (p->offset <= pos) && (pos < (p->offset+p->size))) {
//      // bump the count
//      p->readcount++;
//  } else {
//      // add a new block
//      pst_block_recorder *tail = *t;
//      p = (pst_block_recorder*)xmalloc(sizeof(*p));
//      *t = p;
//      p->next      = tail;
//      p->offset    = pos;
//      p->size      = size;
//      p->readcount = 1;
//  }
//  DEBUG_MAIN(("pst file old offset %#"PRIx64" old size %#x read count %i offset %#"PRIx64" size %#x\n",
//              p->offset, p->size, p->readcount, pos, size));

    if (fseeko(pf->fp, pos, SEEK_SET) == -1) {
        DEBUG_RET();
        return 0;
    }
    rc = fread(buf, (size_t)1, size, pf->fp);
    DEBUG_RET();
    return rc;
}


/**
 * Get an ID block from file using _pst_ff_getIDblock and decrypt if necessary
 *
 * @param pf   PST file structure
 * @param id   ID of block to retrieve
 * @param buf  Reference to pointer that will be set to new block. Any memory
               pointed to by buffer will be free()d beforehand
 * @return     Size of block pointed to by *b
 */
size_t pst_ff_getIDblock_dec(pst_file *pf, uint64_t id, char **buf) {
    size_t r;
    int noenc = (int)(id & 2);   // disable encryption
    DEBUG_ENT("pst_ff_getIDblock_dec");
    DEBUG_INDEX(("for id %#x\n", id));
    r = pst_ff_getIDblock(pf, id, buf);
    if ((pf->encryption) && !(noenc)) {
        (void)pst_decrypt(id, *buf, r, pf->encryption);
    }
    DEBUG_HEXDUMPC(*buf, r, 16);
    DEBUG_RET();
    return r;
}


/**
 * Read a block of data from file into memory
 * @param pf   PST file
 * @param id   identifier of block to read
 * @param buf  reference to pointer to buffer. If this pointer
               is non-NULL, it will first be free()d
 * @return     size of block read into memory
 */
size_t pst_ff_getIDblock(pst_file *pf, uint64_t id, char** buf) {
    pst_index_ll *rec;
    size_t rsize;
    DEBUG_ENT("pst_ff_getIDblock");
    rec = pst_getID(pf, id);
    if (!rec) {
        DEBUG_INDEX(("Cannot find ID %#"PRIx64"\n", id));
        DEBUG_RET();
        return 0;
    }
    DEBUG_INDEX(("id = %#"PRIx64", record size = %#x, offset = %#x\n", id, rec->size, rec->offset));
    rsize = pst_read_block_size(pf, rec->offset, rec->size, buf);
    DEBUG_RET();
    return rsize;
}


#define PST_PTR_BLOCK_SIZE 0x120
size_t pst_ff_getID2block(pst_file *pf, uint64_t id2, pst_index2_ll *id2_head, char** buf) {
    size_t ret;
    pst_index_ll* ptr;
    pst_holder h = {buf, NULL, 0};
    DEBUG_ENT("pst_ff_getID2block");
    ptr = pst_getID2(id2_head, id2);

    if (!ptr) {
        DEBUG_INDEX(("Cannot find id2 value %#x\n", id2));
        DEBUG_RET();
        return 0;
    }
    ret = pst_ff_getID2data(pf, ptr, &h);
    DEBUG_RET();
    return ret;
}


size_t pst_ff_getID2data(pst_file *pf, pst_index_ll *ptr, pst_holder *h) {
    size_t ret;
    char *b = NULL, *t;
    DEBUG_ENT("pst_ff_getID2data");
    if (!(ptr->id & 0x02)) {
        ret = pst_ff_getIDblock_dec(pf, ptr->id, &b);
        if (h->buf) {
            *(h->buf) = b;
        } else if ((h->base64 == 1) && h->fp) {
            t = base64_encode(b, ret);
            if (t) {
                (void)pst_fwrite(t, (size_t)1, strlen(t), h->fp);
                free(t);    // caught by valgrind
            }
            free(b);
        } else if (h->fp) {
            (void)pst_fwrite(b, (size_t)1, ret, h->fp);
            free(b);
        } else {
            // h-> does not specify any output
        }

    } else {
        // here we will assume it is a block that points to others
        DEBUG_READ(("Assuming it is a multi-block record because of it's id\n"));
        ret = pst_ff_compile_ID(pf, ptr->id, h, (size_t)0);
    }
    DEBUG_RET();
    return ret;
}


size_t pst_ff_compile_ID(pst_file *pf, uint64_t id, pst_holder *h, size_t size) {
    size_t z, a, b;
    uint16_t count, y;
    char *buf3 = NULL, *buf2 = NULL, *t;
    char *b_ptr;
    int  line_count = 0;
    char      base64_extra_chars[3];
    uint32_t  base64_extra = 0;
    pst_block_hdr  block_hdr;
    pst_table3_rec table3_rec;  //for type 3 (0x0101) blocks

    DEBUG_ENT("pst_ff_compile_ID");
    a = pst_ff_getIDblock(pf, id, &buf3);
    if (!a) {
        if (buf3) free(buf3);
        DEBUG_RET();
        return 0;
    }
    DEBUG_HEXDUMPC(buf3, a, 0x10);
    memcpy(&block_hdr, buf3, sizeof(block_hdr));
    LE16_CPU(block_hdr.index_offset);
    LE16_CPU(block_hdr.type);
    LE32_CPU(block_hdr.offset);
    DEBUG_EMAIL(("block header (index_offset=%#hx, type=%#hx, offset=%#x)\n", block_hdr.index_offset, block_hdr.type, block_hdr.offset));

    if (block_hdr.index_offset != (uint16_t)0x0101) { //type 3
        DEBUG_WARN(("WARNING: not a type 0x0101 buffer, Treating as normal buffer\n"));
        if (pf->encryption) (void)pst_decrypt(id, buf3, a, pf->encryption);
        if (h->buf)
            *(h->buf) = buf3;
        else if (h->base64 == 1 && h->fp) {
            t = base64_encode(buf3, a);
            if (t) {
                (void)pst_fwrite(t, (size_t)1, strlen(t), h->fp);
                free(t);    // caught by valgrind
            }
            free(buf3);
        } else if (h->fp) {
            (void)pst_fwrite(buf3, (size_t)1, a, h->fp);
            free(buf3);
        } else {
            // h-> does not specify any output
        }
        DEBUG_RET();
        return a;
    }
    count = block_hdr.type;
    b_ptr = buf3 + 8;
    line_count = 0;
    for (y=0; y<count; y++) {
        b_ptr += pst_decode_type3(pf, &table3_rec, b_ptr);
        z = pst_ff_getIDblock_dec(pf, table3_rec.id, &buf2);
        if (!z) {
            DEBUG_WARN(("call to getIDblock returned zero %i\n", z));
            if (buf2) free(buf2);
            free(buf3);
            DEBUG_RET();
            return z;
        }
        if (h->buf) {
            *(h->buf) = realloc(*(h->buf), size+z+1);
            DEBUG_READ(("appending read data of size %i onto main buffer from pos %i\n", z, size));
            memcpy(&((*(h->buf))[size]), buf2, z);
        } else if ((h->base64 == 1) && h->fp) {
            if (base64_extra) {
                // include any bytes left over from the last encoding
                buf2 = (char*)realloc(buf2, z+base64_extra);
                memmove(buf2+base64_extra, buf2, z);
                memcpy(buf2, base64_extra_chars, base64_extra);
                z += base64_extra;
            }

            // find out how many bytes will be left over after this encoding and save them
            base64_extra = z % 3;
            if (base64_extra) {
                z -= base64_extra;
                memcpy(base64_extra_chars, buf2+z, base64_extra);
            }

            // encode this chunk
            t = base64_encode_multiple(buf2, z, &line_count);
            if (t) {
                DEBUG_READ(("writing %i bytes to file as base64 [%i]. Currently %i\n", z, strlen(t), size));
                (void)pst_fwrite(t, (size_t)1, strlen(t), h->fp);
                free(t);    // caught by valgrind
            }
        } else if (h->fp) {
            DEBUG_READ(("writing %i bytes to file. Currently %i\n", z, size));
            (void)pst_fwrite(buf2, (size_t)1, z, h->fp);
        } else {
            // h-> does not specify any output
        }
        size += z;
    }
    if ((h->base64 == 1) && h->fp && base64_extra) {
        // need to encode any bytes left over
        t = base64_encode_multiple(base64_extra_chars, (size_t)base64_extra, &line_count);
        if (t) {
            (void)pst_fwrite(t, (size_t)1, strlen(t), h->fp);
            free(t);    // caught by valgrind
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

int pst_stricmp(char *a, char *b) {
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


int pst_strincmp(char *a, char *b, size_t x) {
    // compare upto x chars in string a and b case-insensitively
    // returns -1 if a < b, 0 if a==b, 1 if a > b
    size_t y = 0;
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


size_t pst_fwrite(const void* ptr, size_t size, size_t nmemb, FILE *stream) {
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


char * pst_wide_to_single(char *wt, size_t size) {
    // returns the first byte of each wide char. the size is the number of bytes in source
    char *x, *y;
    DEBUG_ENT("pst_wide_to_single");
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


char *pst_rfc2426_escape(char *str) {
    static char*  buf    = NULL;
    static size_t buflen = 0;
    char *ret, *a, *b;
    size_t x = 0;
    int y, z;
    DEBUG_ENT("rfc2426_escape");
    if (!str)
        ret = str;
    else {

        // calculate space required to escape all the following characters
        y = pst_chr_count(str, ',')
          + pst_chr_count(str, '\\')
          + pst_chr_count(str, ';')
          + pst_chr_count(str, '\n');
        z = pst_chr_count(str, '\r');
        if (y == 0 && z == 0)
            // there isn't any extra space required
            ret = str;
        else {
            x = strlen(str) + y - z + 1; // don't forget room for the NUL
            if (x > buflen) {
                buf = (char*) realloc(buf, x);
                buflen = x;
            }
            a = str;
            b = buf;
            while (*a != '\0') {
                switch (*a) {
                case ',' :
                case '\\':
                case ';' :
                    *(b++) = '\\';
                    *b = *a;
                    break;
                case '\n':  // newlines are encoded as "\n"
                    *(b++) = '\\';
                    *b = 'n';
                    break;
                case '\r':  // skip cr
                    b--;
                    break;
                default:
                    *b=*a;
                }
                b++;
                a++;
            }
            *b = '\0'; // NUL-terminate the string (buf)
            ret = buf;
        }
    }
    DEBUG_RET();
    return ret;
}


int pst_chr_count(char *str, char x) {
    int r = 0;
    while (*str) {
        if (*str == x) r++;
        str++;
    }
    return r;
}


char *pst_rfc2425_datetime_format(FILETIME *ft) {
    static char buffer[30];
    struct tm *stm = NULL;
    DEBUG_ENT("rfc2425_datetime_format");
    stm = fileTimeToStructTM(ft);
    if (strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", stm)==0) {
        DEBUG_INFO(("Problem occured formatting date\n"));
    }
    DEBUG_RET();
    return buffer;
}


char *pst_rfc2445_datetime_format(FILETIME *ft) {
    static char buffer[30];
    struct tm *stm = NULL;
    DEBUG_ENT("rfc2445_datetime_format");
    stm = fileTimeToStructTM(ft);
    if (strftime(buffer, sizeof(buffer), "%Y%m%dT%H%M%SZ", stm)==0) {
        DEBUG_INFO(("Problem occured formatting date\n"));
    }
    DEBUG_RET();
    return buffer;
}


