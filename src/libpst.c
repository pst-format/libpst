/***
 * libpst.c
 * Part of the LibPST project
 * Written by David Smith
 *            dave.s@earthcorp.com
 */

#include "define.h"


// switch to maximal packing for our own internal structures
// use the same code as in libpst.h
#ifdef _MSC_VER
    #pragma pack(push, 1)
#endif
#if defined(__GNUC__) || defined (__SUNPRO_C) || defined(__SUNPRO_CC)
    #pragma pack(1)
#endif

#define ASSERT(x) { if(!(x)) raise( SIGSEGV ); }

#define INDEX_TYPE32            0x0E
#define INDEX_TYPE32A           0x0F    // unknown, but assumed to be similar for now
#define INDEX_TYPE64            0x17
#define INDEX_TYPE64A           0x15    // http://sourceforge.net/projects/libpff/
#define INDEX_TYPE_OFFSET       (int64_t)0x0A

#define FILE_SIZE_POINTER32     (int64_t)0xA8
#define INDEX_POINTER32         (int64_t)0xC4
#define INDEX_BACK32            (int64_t)0xC0
#define SECOND_POINTER32        (int64_t)0xBC
#define SECOND_BACK32           (int64_t)0xB8
#define ENC_TYPE32              (int64_t)0x1CD

#define FILE_SIZE_POINTER64     (int64_t)0xB8
#define INDEX_POINTER64         (int64_t)0xF0
#define INDEX_BACK64            (int64_t)0xE8
#define SECOND_POINTER64        (int64_t)0xE0
#define SECOND_BACK64           (int64_t)0xD8
#define ENC_TYPE64              (int64_t)0x201

#define FILE_SIZE_POINTER ((pf->do_read64) ? FILE_SIZE_POINTER64 : FILE_SIZE_POINTER32)
#define INDEX_POINTER     ((pf->do_read64) ? INDEX_POINTER64     : INDEX_POINTER32)
#define INDEX_BACK        ((pf->do_read64) ? INDEX_BACK64        : INDEX_BACK32)
#define SECOND_POINTER    ((pf->do_read64) ? SECOND_POINTER64    : SECOND_POINTER32)
#define SECOND_BACK       ((pf->do_read64) ? SECOND_BACK64       : SECOND_BACK32)
#define ENC_TYPE          ((pf->do_read64) ? ENC_TYPE64          : ENC_TYPE32)

#define PST_SIGNATURE 0x4E444221


typedef struct pst_block_offset {
    uint16_t from;
    uint16_t to;
} pst_block_offset;


typedef struct pst_block_offset_pointer {
    char *from;
    char *to;
    int   needfree;
} pst_block_offset_pointer;


typedef struct pst_holder {
    char  **buf;
    FILE   *fp;
    int     base64;                 // bool, are we encoding into base64
    int     base64_line_count;      // base64 bytes emitted on the current line
    size_t  base64_extra;           // count of bytes held in base64_extra_chars
    char    base64_extra_chars[2];  // up to two pending unencoded bytes
} pst_holder;


typedef struct pst_subblock {
    char    *buf;
    size_t   read_size;
    size_t   i_offset;
} pst_subblock;


typedef struct pst_subblocks {
    size_t          subblock_count;
    pst_subblock   *subs;
} pst_subblocks;


typedef struct pst_mapi_element {
    uint32_t   mapi_id;
    char      *data;
    uint32_t   type;
    size_t     size;
    char      *extra;
} pst_mapi_element;


typedef struct pst_mapi_object {
    int32_t count_elements;     // count of active elements
    int32_t orig_count;         // originally allocated elements
    int32_t count_objects;      // number of mapi objects in the list
    struct pst_mapi_element **elements;
    struct pst_mapi_object *next;
} pst_mapi_object;


typedef struct pst_desc32 {
    uint32_t d_id;
    uint32_t desc_id;
    uint32_t tree_id;
    uint32_t parent_d_id;
} pst_desc32;


typedef struct pst_index32 {
    uint32_t id;
    uint32_t offset;
    uint16_t size;
    int16_t  u1;
} pst_index32;


struct pst_table_ptr_struct32{
  uint32_t start;
  uint32_t u1;
  uint32_t offset;
};


typedef struct pst_desc {
    uint64_t d_id;
    uint64_t desc_id;
    uint64_t tree_id;
    uint32_t parent_d_id;   // not 64 bit
    uint32_t u1;            // padding
} pst_desc;


typedef struct pst_index {
    uint64_t id;
    uint64_t offset;
    uint16_t size;
    int16_t  u0;
    int32_t  u1;
} pst_index;


struct pst_table_ptr_struct{
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
    uint32_t child_id;
} pst_id2_assoc32;


typedef struct pst_id2_assoc {
    uint32_t id2;       // only 32 bit here
    uint16_t unknown1;
    uint16_t unknown2;
    uint64_t id;
    uint64_t child_id;
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


/** for "compressible" encryption, just a simple substitution cipher,
 *  plaintext = comp_enc[ciphertext];
 *  for "strong" encryption, this is the first rotor of an Enigma 3 rotor cipher.
 */
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

/** for "strong" encryption, this is the second rotor of an Enigma 3 rotor cipher.
 */
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

/** for "strong" encryption, this is the third rotor of an Enigma 3 rotor cipher.
 */
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

static size_t           pst_append_holder(pst_holder *h, size_t size, char **buf, size_t z);
static int              pst_build_desc_ptr(pst_file *pf, int64_t offset, int32_t depth, uint64_t linku1, uint64_t start_val, uint64_t end_val);
static pst_id2_tree*    pst_build_id2(pst_file *pf, pst_index_ll* list);
static int              pst_build_id_ptr(pst_file *pf, int64_t offset, int32_t depth, uint64_t linku1, uint64_t start_val, uint64_t end_val);
static int              pst_chr_count(char *str, char x);
static size_t           pst_ff_compile_ID(pst_file *pf, uint64_t i_id, pst_holder *h, size_t size);
static size_t           pst_ff_getIDblock(pst_file *pf, uint64_t i_id, char** buf);
static size_t           pst_ff_getID2block(pst_file *pf, uint64_t id2, pst_id2_tree *id2_head, char** buf);
static size_t           pst_ff_getID2data(pst_file *pf, pst_index_ll *ptr, pst_holder *h);
static size_t           pst_finish_cleanup_holder(pst_holder *h, size_t size);
static void             pst_free_attach(pst_item_attach *attach);
static void             pst_free_desc (pst_desc_tree *head);
static void             pst_free_id2(pst_id2_tree * head);
static void             pst_free_id (pst_index_ll *head);
static void             pst_free_list(pst_mapi_object *list);
static void             pst_free_xattrib(pst_x_attrib_ll *x);
static size_t           pst_getAtPos(pst_file *pf, int64_t pos, void* buf, size_t size);
static int              pst_getBlockOffsetPointer(pst_file *pf, pst_id2_tree *i2_head, pst_subblocks *subblocks, uint32_t offset, pst_block_offset_pointer *p);
static int              pst_getBlockOffset(char *buf, size_t read_size, uint32_t i_offset, uint32_t offset, pst_block_offset *p);
static pst_id2_tree*    pst_getID2(pst_id2_tree * ptr, uint64_t id);
static pst_desc_tree*   pst_getDptr(pst_file *pf, uint64_t d_id);
static uint64_t         pst_getIntAt(pst_file *pf, char *buf);
static uint64_t         pst_getIntAtPos(pst_file *pf, int64_t pos);
static pst_mapi_object* pst_parse_block(pst_file *pf, uint64_t block_id, pst_id2_tree *i2_head);
static void             pst_printDptr(pst_file *pf, pst_desc_tree *ptr);
static void             pst_printID2ptr(pst_id2_tree *ptr);
static int              pst_process(uint64_t block_id, pst_mapi_object *list, pst_item *item, pst_item_attach *attach);
static size_t           pst_read_block_size(pst_file *pf, int64_t offset, size_t size, char **buf);
static int              pst_decrypt(uint64_t i_id, char *buf, size_t size, unsigned char type);
static int              pst_stricmp(char *a, char *b);
static int              pst_strincmp(char *a, char *b, size_t x);
static char*            pst_wide_to_single(char *wt, size_t size);



int pst_open(pst_file *pf, const char *name, const char *charset) {
    int32_t sig;

    pst_unicode_init();

    DEBUG_ENT("pst_open");

    if (!pf) {
        WARN (("cannot be passed a NULL pst_file\n"));
        DEBUG_RET();
        return -1;
    }
    memset(pf, 0, sizeof(*pf));
    pf->charset = charset;

    if ((pf->fp = fopen(name, "rb")) == NULL) {
        perror("Error opening PST file");
        DEBUG_RET();
        return -1;
    }

    // Check pst file magic
    if (pst_getAtPos(pf, 0, &sig, sizeof(sig)) != sizeof(sig)) {
        (void)fclose(pf->fp);
        DEBUG_WARN(("cannot read signature from PST file. Closing with error\n"));
        DEBUG_RET();
        return -1;
    }
    LE32_CPU(sig);
    DEBUG_INFO(("sig = %X\n", sig));
    if (sig != (int32_t)PST_SIGNATURE) {
        (void)fclose(pf->fp);
        DEBUG_WARN(("not a PST file that I know. Closing with error\n"));
        DEBUG_RET();
        return -1;
    }

    // read index type
    (void)pst_getAtPos(pf, INDEX_TYPE_OFFSET, &(pf->ind_type), sizeof(pf->ind_type));
    DEBUG_INFO(("index_type = %i\n", pf->ind_type));
    switch (pf->ind_type) {
        case INDEX_TYPE32 :
        case INDEX_TYPE32A :
            pf->do_read64 = 0;
            break;
        case INDEX_TYPE64 :
        case INDEX_TYPE64A :
            pf->do_read64 = 1;
            break;
        default:
            (void)fclose(pf->fp);
            DEBUG_WARN(("unknown .pst format, possibly newer than Outlook 2003 PST file?\n"));
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

    pf->cwd = pst_malloc(PATH_MAX+1);
    getcwd(pf->cwd, PATH_MAX+1);
    pf->fname = strdup(name);
    return 0;
}


int  pst_reopen(pst_file *pf) {
    char cwd[PATH_MAX];
    if (!getcwd(cwd, PATH_MAX))            return -1;
    if (chdir(pf->cwd))                    return -1;
    if (!freopen(pf->fname, "rb", pf->fp)) return -1;
    if (chdir(cwd))                        return -1;
    return 0;
}


int pst_close(pst_file *pf) {
    DEBUG_ENT("pst_close");
    if (!pf->fp) {
        DEBUG_RET();
        return 0;
    }
    if (fclose(pf->fp)) {
        DEBUG_WARN(("fclose returned non-zero value\n"));
    }
    // free the paths
    free(pf->cwd);
    free(pf->fname);
    // we must free the id linklist and the desc tree
    pst_free_id(pf->i_head);
    pst_free_desc(pf->d_head);
    pst_free_xattrib(pf->x_head);
    DEBUG_RET();
    return 0;
}


/**
 * add a pst descriptor node to a linked list of such nodes.
 *
 * @param node  pointer to the node to be added to the list
 * @param head  pointer to the list head pointer
 * @param tail  pointer to the list tail pointer
 */
static void add_descriptor_to_list(pst_desc_tree *node, pst_desc_tree **head, pst_desc_tree **tail);
static void add_descriptor_to_list(pst_desc_tree *node, pst_desc_tree **head, pst_desc_tree **tail)
{
    DEBUG_ENT("add_descriptor_to_list");
    //DEBUG_INFO(("Added node %#"PRIx64" parent %#"PRIx64" real parent %#"PRIx64" prev %#"PRIx64" next %#"PRIx64"\n",
    //             node->id, node->parent_d_id,
    //             (node->parent ? node->parent->id : (uint64_t)0),
    //             (node->prev   ? node->prev->id   : (uint64_t)0),
    //             (node->next   ? node->next->id   : (uint64_t)0)));
    if (*tail) (*tail)->next = node;
    if (!(*head)) *head = node;
    node->prev = *tail;
    node->next = NULL;
    *tail = node;
    DEBUG_RET();
}


/**
 * add a pst descriptor node into the global tree.
 *
 * @param pf   global pst file pointer
 * @param node pointer to the new node to be added to the tree
 */
static void record_descriptor(pst_file *pf, pst_desc_tree *node);
static void record_descriptor(pst_file *pf, pst_desc_tree *node)
{
    DEBUG_ENT("record_descriptor");
    // finish node initialization
    node->parent     = NULL;
    node->child      = NULL;
    node->child_tail = NULL;
    node->no_child   = 0;

    // find any orphan children of this node, and collect them
    pst_desc_tree *n = pf->d_head;
    while (n) {
        if (n->parent_d_id == node->d_id) {
            // found a child of this node
            DEBUG_INFO(("Found orphan child %#"PRIx64" of parent %#"PRIx64"\n", n->d_id, node->d_id));
            pst_desc_tree *nn = n->next;
            pst_desc_tree *pp = n->prev;
            node->no_child++;
            n->parent = node;
            add_descriptor_to_list(n, &node->child, &node->child_tail);
            if (pp) pp->next = nn; else pf->d_head = nn;
            if (nn) nn->prev = pp; else pf->d_tail = pp;
            n = nn;
        }
        else {
            n = n->next;
        }
    }

    // now hook this node into the global tree
    if (node->parent_d_id == 0) {
        // add top level node to the descriptor tree
        //DEBUG_INFO(("Null parent\n"));
        add_descriptor_to_list(node, &pf->d_head, &pf->d_tail);
    }
    else if (node->parent_d_id == node->d_id) {
        // add top level node to the descriptor tree
        DEBUG_INFO(("%#"PRIx64" is its own parent. What is this world coming to?\n", node->d_id));
        add_descriptor_to_list(node, &pf->d_head, &pf->d_tail);
    } else {
        //DEBUG_INFO(("Searching for parent %#"PRIx64" of %#"PRIx64"\n", node->parent_d_id, node->d_id));
        pst_desc_tree *parent = pst_getDptr(pf, node->parent_d_id);
        if (parent) {
            //DEBUG_INFO(("Found parent %#"PRIx64"\n", node->parent_d_id));
            parent->no_child++;
            node->parent = parent;
            add_descriptor_to_list(node, &parent->child, &parent->child_tail);
        }
        else {
            DEBUG_INFO(("No parent %#"PRIx64", have an orphan child %#"PRIx64"\n", node->parent_d_id, node->d_id));
            add_descriptor_to_list(node, &pf->d_head, &pf->d_tail);
        }
    }
    DEBUG_RET();
}


/**
 * make a deep copy of part of the id2 mapping tree, for use
 * by an attachment containing an embedded rfc822 message.
 *
 * @param   head  pointer to the subtree to be copied
 * @return        pointer to the new copy of the subtree
 */
static pst_id2_tree* deep_copy(pst_id2_tree *head);
static pst_id2_tree* deep_copy(pst_id2_tree *head)
{
    if (!head) return NULL;
    pst_id2_tree* me = (pst_id2_tree*) pst_malloc(sizeof(pst_id2_tree));
    me->id2 = head->id2;
    me->id  = head->id;
    me->child = deep_copy(head->child);
    me->next  = deep_copy(head->next);
    return me;
}


pst_desc_tree* pst_getTopOfFolders(pst_file *pf, const pst_item *root) {
    pst_desc_tree *topnode;
    uint32_t topid;
    DEBUG_ENT("pst_getTopOfFolders");
    if (!root || !root->message_store) {
        DEBUG_INFO(("There isn't a top of folder record here.\n"));
        DEBUG_RET();
        return NULL;
    }
    if (!root->message_store->top_of_personal_folder) {
        // this is the OST way
        // ASSUMPTION: Top Of Folders record in PST files is *always* descid 0x2142
        topid = 0x2142;
    } else {
        topid = root->message_store->top_of_personal_folder->id;
    }
    DEBUG_INFO(("looking for top of folder descriptor %#"PRIx32"\n", topid));
    topnode = pst_getDptr(pf, (uint64_t)topid);
    if (!topnode) {
        // add dummy top record to pickup orphan children
        topnode              = (pst_desc_tree*) pst_malloc(sizeof(pst_desc_tree));
        topnode->d_id        = topid;
        topnode->parent_d_id = 0;
        topnode->assoc_tree  = NULL;
        topnode->desc        = NULL;
        record_descriptor(pf, topnode);   // add to the global tree
    }
    DEBUG_RET();
    return topnode;
}


pst_binary pst_attach_to_mem(pst_file *pf, pst_item_attach *attach) {
    pst_index_ll *ptr;
    pst_binary rc;
    pst_holder h = {&rc.data, NULL, 0, 0, 0};
    rc.size = 0;
    rc.data = NULL;
    DEBUG_ENT("pst_attach_to_mem");
    if ((!attach->data.data) && (attach->i_id != (uint64_t)-1)) {
        ptr = pst_getID(pf, attach->i_id);
        if (ptr) {
            rc.size = pst_ff_getID2data(pf, ptr, &h);
        } else {
            DEBUG_WARN(("Couldn't find ID pointer. Cannot handle attachment\n"));
        }
    } else {
        rc = attach->data;
        attach->data.data = NULL;   // prevent pst_free_item() from trying to free this
        attach->data.size = 0;      // since we have given that buffer to the caller
    }
    DEBUG_RET();
    return rc;
}


size_t pst_attach_to_file(pst_file *pf, pst_item_attach *attach, FILE* fp) {
    pst_index_ll *ptr;
    pst_holder h = {NULL, fp, 0, 0, 0};
    size_t size = 0;
    DEBUG_ENT("pst_attach_to_file");
    if ((!attach->data.data) && (attach->i_id != (uint64_t)-1)) {
        ptr = pst_getID(pf, attach->i_id);
        if (ptr) {
            size = pst_ff_getID2data(pf, ptr, &h);
        } else {
            DEBUG_WARN(("Couldn't find ID pointer. Cannot save attachment to file\n"));
        }
    } else {
        size = attach->data.size;
        if (attach->data.data && size) {
            // save the attachment to the file
            (void)pst_fwrite(attach->data.data, (size_t)1, size, fp);
        }
    }
    DEBUG_RET();
    return size;
}


size_t pst_attach_to_file_base64(pst_file *pf, pst_item_attach *attach, FILE* fp) {
    pst_index_ll *ptr;
    pst_holder h = {NULL, fp, 1, 0, 0};
    size_t size = 0;
    DEBUG_ENT("pst_attach_to_file_base64");
    if ((!attach->data.data) && (attach->i_id != (uint64_t)-1)) {
        ptr = pst_getID(pf, attach->i_id);
        if (ptr) {
            size = pst_ff_getID2data(pf, ptr, &h);
        } else {
            DEBUG_WARN(("Couldn't find ID pointer. Cannot save attachment to Base64\n"));
        }
    } else {
        size = attach->data.size;
        if (attach->data.data && size) {
            // encode the attachment to the file
            char *c = pst_base64_encode(attach->data.data, size);
            if (c) {
                (void)pst_fwrite(c, (size_t)1, strlen(c), fp);
                free(c);    // caught by valgrind
            }
        }
    }
    DEBUG_RET();
    return size;
}


int pst_load_index (pst_file *pf) {
    int  x;
    DEBUG_ENT("pst_load_index");
    if (!pf) {
        DEBUG_WARN(("Cannot load index for a NULL pst_file\n"));
        DEBUG_RET();
        return -1;
    }

    x = pst_build_id_ptr(pf, pf->index1, 0, pf->index1_back, 0, UINT64_MAX);
    DEBUG_INFO(("build id ptr returns %i\n", x));

    x = pst_build_desc_ptr(pf, pf->index2, 0, pf->index2_back, (uint64_t)0x21, UINT64_MAX);
    DEBUG_INFO(("build desc ptr returns %i\n", x));

    pst_printDptr(pf, pf->d_head);

    DEBUG_RET();
    return 0;
}


pst_desc_tree* pst_getNextDptr(pst_desc_tree* d) {
    pst_desc_tree* r = NULL;
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
    uint32_t extended;
    uint16_t type;
    uint16_t map;
} pst_x_attrib;


/** Try to load the extended attributes from the pst file.
    @return true(1) or false(0) to indicate whether the extended attributes have been loaded
 */
int pst_load_extended_attributes(pst_file *pf) {
    // for PST files this will load up d_id 0x61 and check it's "assoc_tree" attribute.
    pst_desc_tree *p;
    pst_mapi_object *list;
    pst_id2_tree *id2_head = NULL;
    char *buffer=NULL, *headerbuffer=NULL;
    size_t bsize=0, hsize=0, bptr=0;
    pst_x_attrib xattrib;
    int32_t tint, x;
    pst_x_attrib_ll *ptr, *p_head=NULL;

    DEBUG_ENT("pst_loadExtendedAttributes");
    p = pst_getDptr(pf, (uint64_t)0x61);
    if (!p) {
        DEBUG_WARN(("Cannot find d_id 0x61 for loading the Extended Attributes\n"));
        DEBUG_RET();
        return 0;
    }

    if (!p->desc) {
        DEBUG_WARN(("descriptor is NULL for d_id 0x61. Cannot load Extended Attributes\n"));
        DEBUG_RET();
        return 0;
    }

    if (p->assoc_tree) {
        id2_head = pst_build_id2(pf, p->assoc_tree);
        pst_printID2ptr(id2_head);
    } else {
        DEBUG_WARN(("Have not been able to fetch any id2 values for d_id 0x61. Brace yourself!\n"));
    }

    list = pst_parse_block(pf, p->desc->i_id, id2_head);
    if (!list) {
        DEBUG_WARN(("Cannot process desc block for item 0x61. Not loading extended Attributes\n"));
        pst_free_id2(id2_head);
        DEBUG_RET();
        return 0;
    }

    DEBUG_INFO(("look thru d_id 0x61 list of mapi objects\n"));
    for (x=0; x < list->count_elements; x++) {
        DEBUG_INFO(("#%d - mapi-id: %#x type: %#x length: %#x\n", x, list->elements[x]->mapi_id, list->elements[x]->type, list->elements[x]->size));
        if (list->elements[x]->data) {
            DEBUG_HEXDUMPC(list->elements[x]->data, list->elements[x]->size, 0x10);
        }
        if (list->elements[x]->mapi_id == (uint32_t)0x0003) {
            buffer = list->elements[x]->data;
            bsize  = list->elements[x]->size;
        } else if (list->elements[x]->mapi_id == (uint32_t)0x0004) {
            headerbuffer = list->elements[x]->data;
            hsize        = list->elements[x]->size;
        } else {
            // leave them null
        }
    }

    if (!buffer) {
        pst_free_list(list);
        DEBUG_WARN(("No extended attributes buffer found. Not processing\n"));
        DEBUG_RET();
        return 0;
    }

    while (bptr < bsize) {
        int err = 0;
        xattrib.extended= PST_LE_GET_UINT32(buffer+bptr), bptr += 4;
        xattrib.type    = PST_LE_GET_UINT16(buffer+bptr), bptr += 2;
        xattrib.map     = PST_LE_GET_UINT16(buffer+bptr), bptr += 2;
        ptr = (pst_x_attrib_ll*) pst_malloc(sizeof(*ptr));
        memset(ptr, 0, sizeof(*ptr));
        ptr->map  = xattrib.map+0x8000;
        ptr->next = NULL;
        DEBUG_INFO(("xattrib: ext = %#"PRIx32", type = %#"PRIx16", map = %#"PRIx16"\n",
             xattrib.extended, xattrib.type, xattrib.map));
        if (xattrib.type & 0x0001) { // if the Bit 1 is set
            // pointer to Unicode field in buffer
            if (xattrib.extended < hsize) {
                char *wt;
                // copy the size of the header. It is 32 bit int
                memcpy(&tint, &(headerbuffer[xattrib.extended]), sizeof(tint));
                LE32_CPU(tint);
                wt = (char*) pst_malloc((size_t)(tint+2)); // plus 2 for a uni-code zero
                memset(wt, 0, (size_t)(tint+2));
                memcpy(wt, &(headerbuffer[xattrib.extended+sizeof(tint)]), (size_t)tint);
                ptr->data = pst_wide_to_single(wt, (size_t)tint);
                free(wt);
                DEBUG_INFO(("Mapped attribute %#"PRIx32" to %s\n", ptr->map, ptr->data));
            } else {
                DEBUG_INFO(("Cannot read outside of buffer [%i !< %i]\n", xattrib.extended, hsize));
                err = 1;
            }
            ptr->mytype = PST_MAP_HEADER;
        } else {
            // contains the attribute code to map to.
            ptr->data = (uint32_t*)pst_malloc(sizeof(uint32_t));
            memset(ptr->data, 0, sizeof(uint32_t));
            *((uint32_t*)ptr->data) = xattrib.extended;
            ptr->mytype = PST_MAP_ATTRIB;
            DEBUG_INFO(("Mapped attribute %#"PRIx32" to %#"PRIx32"\n", ptr->map, *((uint32_t*)ptr->data)));
        }

        if (!err) {
            // add it to the list
            pst_x_attrib_ll *p_sh  = p_head;
            pst_x_attrib_ll *p_sh2 = NULL;
            while (p_sh && (ptr->map > p_sh->map)) {
                p_sh2 = p_sh;
                p_sh  = p_sh->next;
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
        }
    }
    pst_free_id2(id2_head);
    pst_free_list(list);
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


static size_t pst_decode_desc(pst_file *pf, pst_desc *desc, char *buf);
static size_t pst_decode_desc(pst_file *pf, pst_desc *desc, char *buf) {
    size_t r;
    if (pf->do_read64) {
        DEBUG_INFO(("Decoding desc64\n"));
        DEBUG_HEXDUMPC(buf, sizeof(pst_desc), 0x10);
        memcpy(desc, buf, sizeof(pst_desc));
        LE64_CPU(desc->d_id);
        LE64_CPU(desc->desc_id);
        LE64_CPU(desc->tree_id);
        LE32_CPU(desc->parent_d_id);
        LE32_CPU(desc->u1);
        r = sizeof(pst_desc);
    }
    else {
        pst_desc32 d32;
        DEBUG_INFO(("Decoding desc32\n"));
        DEBUG_HEXDUMPC(buf, sizeof(pst_desc32), 0x10);
        memcpy(&d32, buf, sizeof(pst_desc32));
        LE32_CPU(d32.d_id);
        LE32_CPU(d32.desc_id);
        LE32_CPU(d32.tree_id);
        LE32_CPU(d32.parent_d_id);
        desc->d_id        = d32.d_id;
        desc->desc_id     = d32.desc_id;
        desc->tree_id     = d32.tree_id;
        desc->parent_d_id = d32.parent_d_id;
        desc->u1          = 0;
        r = sizeof(pst_desc32);
    }
    return r;
}


static size_t pst_decode_table(pst_file *pf, struct pst_table_ptr_struct *table, char *buf);
static size_t pst_decode_table(pst_file *pf, struct pst_table_ptr_struct *table, char *buf) {
    size_t r;
    if (pf->do_read64) {
        DEBUG_INFO(("Decoding table64\n"));
        DEBUG_HEXDUMPC(buf, sizeof(struct pst_table_ptr_struct), 0x10);
        memcpy(table, buf, sizeof(struct pst_table_ptr_struct));
        LE64_CPU(table->start);
        LE64_CPU(table->u1);
        LE64_CPU(table->offset);
        r =sizeof(struct pst_table_ptr_struct);
    }
    else {
        struct pst_table_ptr_struct32 t32;
        DEBUG_INFO(("Decoding table32\n"));
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
        DEBUG_INFO(("Decoding index64\n"));
        DEBUG_HEXDUMPC(buf, sizeof(pst_index), 0x10);
        memcpy(index, buf, sizeof(pst_index));
        LE64_CPU(index->id);
        LE64_CPU(index->offset);
        LE16_CPU(index->size);
        LE16_CPU(index->u0);
        LE32_CPU(index->u1);
        r = sizeof(pst_index);
    } else {
        pst_index32 index32;
        DEBUG_INFO(("Decoding index32\n"));
        DEBUG_HEXDUMPC(buf, sizeof(pst_index32), 0x10);
        memcpy(&index32, buf, sizeof(pst_index32));
        LE32_CPU(index32.id);
        LE32_CPU(index32.offset);
        LE16_CPU(index32.size);
        LE16_CPU(index32.u1);
        index->id     = index32.id;
        index->offset = index32.offset;
        index->size   = index32.size;
        index->u0     = 0;
        index->u1     = index32.u1;
        r = sizeof(pst_index32);
    }
    return r;
}


static size_t pst_decode_assoc(pst_file *pf, pst_id2_assoc *assoc, char *buf);
static size_t pst_decode_assoc(pst_file *pf, pst_id2_assoc *assoc, char *buf) {
    size_t r;
    if (pf->do_read64) {
        DEBUG_INFO(("Decoding assoc64\n"));
        DEBUG_HEXDUMPC(buf, sizeof(pst_id2_assoc), 0x10);
        memcpy(assoc, buf, sizeof(pst_id2_assoc));
        LE32_CPU(assoc->id2);
        LE64_CPU(assoc->id);
        LE64_CPU(assoc->child_id);
        r = sizeof(pst_id2_assoc);
    } else {
        pst_id2_assoc32 assoc32;
        DEBUG_INFO(("Decoding assoc32\n"));
        DEBUG_HEXDUMPC(buf, sizeof(pst_id2_assoc32), 0x10);
        memcpy(&assoc32, buf, sizeof(pst_id2_assoc32));
        LE32_CPU(assoc32.id2);
        LE32_CPU(assoc32.id);
        LE32_CPU(assoc32.child_id);
        assoc->id2      = assoc32.id2;
        assoc->id       = assoc32.id;
        assoc->child_id = assoc32.child_id;
        r = sizeof(pst_id2_assoc32);
    }
    return r;
}


static size_t pst_decode_type3(pst_file *pf, pst_table3_rec *table3_rec, char *buf);
static size_t pst_decode_type3(pst_file *pf, pst_table3_rec *table3_rec, char *buf) {
    size_t r;
    DEBUG_ENT("pst_decode_type3");
    if (pf->do_read64) {
        DEBUG_INFO(("Decoding table3 64\n"));
        DEBUG_HEXDUMPC(buf, sizeof(pst_table3_rec), 0x10);
        memcpy(table3_rec, buf, sizeof(pst_table3_rec));
        LE64_CPU(table3_rec->id);
        r = sizeof(pst_table3_rec);
    } else {
        pst_table3_rec32 table3_rec32;
        DEBUG_INFO(("Decoding table3 32\n"));
        DEBUG_HEXDUMPC(buf, sizeof(pst_table3_rec32), 0x10);
        memcpy(&table3_rec32, buf, sizeof(pst_table3_rec32));
        LE32_CPU(table3_rec32.id);
        table3_rec->id  = table3_rec32.id;
        r = sizeof(pst_table3_rec32);
    }
    DEBUG_RET();
    return r;
}


/** Process the index1 b-tree from the pst file and create the
 *  pf->i_head linked list from it. This tree holds the location
 *  (offset and size) of lower level objects (0xbcec descriptor
 *  blocks, etc) in the pst file.
 */
static int pst_build_id_ptr(pst_file *pf, int64_t offset, int32_t depth, uint64_t linku1, uint64_t start_val, uint64_t end_val) {
    struct pst_table_ptr_struct table, table2;
    pst_index_ll *i_ptr=NULL;
    pst_index index;
    int32_t x, item_count;
    uint64_t old = start_val;
    char *buf = NULL, *bptr;

    DEBUG_ENT("pst_build_id_ptr");
    DEBUG_INFO(("offset %#"PRIx64" depth %i linku1 %#"PRIx64" start %#"PRIx64" end %#"PRIx64"\n", offset, depth, linku1, start_val, end_val));
    if (end_val <= start_val) {
        DEBUG_WARN(("The end value is BEFORE the start value. This function will quit. Soz. [start:%#"PRIx64", end:%#"PRIx64"]\n", start_val, end_val));
        DEBUG_RET();
        return -1;
    }
    DEBUG_INFO(("Reading index block\n"));
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
            DEBUG_INFO(("[%i]%i Item [id = %#"PRIx64", offset = %#"PRIx64", u1 = %#x, size = %i(%#x)]\n",
                        depth, x, index.id, index.offset, index.u1, index.size, index.size));
            // if (index.id & 0x02) DEBUG_INFO(("two-bit set!!\n"));
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
            i_ptr = (pst_index_ll*) pst_malloc(sizeof(pst_index_ll));
            i_ptr->i_id   = index.id;
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
            DEBUG_INFO(("[%i] %i Index Table [start id = %#"PRIx64", u1 = %#"PRIx64", offset = %#"PRIx64", end id = %#"PRIx64"]\n",
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


/** Process the index2 b-tree from the pst file and create the
 *  pf->d_head tree from it. This tree holds descriptions of the
 *  higher level objects (email, contact, etc) in the pst file.
 */
static int pst_build_desc_ptr (pst_file *pf, int64_t offset, int32_t depth, uint64_t linku1, uint64_t start_val, uint64_t end_val) {
    struct pst_table_ptr_struct table, table2;
    pst_desc desc_rec;
    int32_t item_count;
    uint64_t old = start_val;
    int x;
    char *buf = NULL, *bptr;

    DEBUG_ENT("pst_build_desc_ptr");
    DEBUG_INFO(("offset %#"PRIx64" depth %i linku1 %#"PRIx64" start %#"PRIx64" end %#"PRIx64"\n", offset, depth, linku1, start_val, end_val));
    if (end_val <= start_val) {
        DEBUG_WARN(("The end value is BEFORE the start value. This function will quit. Soz. [start:%#"PRIx64", end:%#"PRIx64"]\n", start_val, end_val));
        DEBUG_RET();
        return -1;
    }
    DEBUG_INFO(("Reading desc block\n"));
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
        for (x=0; x<item_count; x++) {
            bptr += pst_decode_desc(pf, &desc_rec, bptr);
            DEBUG_INFO(("[%i] Item(%#x) = [d_id = %#"PRIx64", desc_id = %#"PRIx64", tree_id = %#"PRIx64", parent_d_id = %#x]\n",
                        depth, x, desc_rec.d_id, desc_rec.desc_id, desc_rec.tree_id, desc_rec.parent_d_id));
            if ((desc_rec.d_id >= end_val) || (desc_rec.d_id < old)) {
                DEBUG_WARN(("This item isn't right. Must be corruption, or I got it wrong!\n"));
                DEBUG_HEXDUMPC(buf, DESC_BLOCK_SIZE, 16);
                if (buf) free(buf);
                DEBUG_RET();
                return -1;
            }
            old = desc_rec.d_id;
            if (x == 0) {   // first entry
                if (start_val && (desc_rec.d_id != start_val)) {
                    DEBUG_WARN(("This item isn't right. Must be corruption, or I got it wrong!\n"));
                    if (buf) free(buf);
                    DEBUG_RET();
                    return -1;
                }
            }
            DEBUG_INFO(("New Record %#"PRIx64" with parent %#x\n", desc_rec.d_id, desc_rec.parent_d_id));
            {
                pst_desc_tree *d_ptr = (pst_desc_tree*) pst_malloc(sizeof(pst_desc_tree));
                d_ptr->d_id        = desc_rec.d_id;
                d_ptr->parent_d_id = desc_rec.parent_d_id;
                d_ptr->assoc_tree  = pst_getID(pf, desc_rec.tree_id);
                d_ptr->desc        = pst_getID(pf, desc_rec.desc_id);
                record_descriptor(pf, d_ptr);   // add to the global tree
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
        for (x=0; x<item_count; x++) {
            bptr += pst_decode_table(pf, &table, bptr);
            if (table.start == 0) break;
            if (x < (item_count-1)) {
                (void)pst_decode_table(pf, &table2, bptr);
            }
            else {
                table2.start = end_val;
            }
            DEBUG_INFO(("[%i] %i Descriptor Table [start id = %#"PRIx64", u1 = %#"PRIx64", offset = %#"PRIx64", end id = %#"PRIx64"]\n",
                        depth, x, table.start, table.u1, table.offset, table2.start));
            if ((table.start >= end_val) || (table.start < old)) {
                DEBUG_WARN(("This table isn't right. Must be corruption, or I got it wrong!\n"));
                if (buf) free(buf);
                DEBUG_RET();
                return -1;
            }
            old = table.start;
            if (x == 0) {   // first entry
                if (start_val && (table.start != start_val)) {
                    DEBUG_WARN(("This table isn't right. Must be corruption, or I got it wrong!\n"));
                    if (buf) free(buf);
                    DEBUG_RET();
                    return -1;
                }
            }
            (void)pst_build_desc_ptr(pf, table.offset, depth+1, table.u1, table.start, table2.start);
        }
    }
    if (buf) free(buf);
    DEBUG_RET();
    return 0;
}


/** Process a high level object from the pst file.
 */
pst_item* pst_parse_item(pst_file *pf, pst_desc_tree *d_ptr, pst_id2_tree *m_head) {
    pst_mapi_object * list;
    pst_id2_tree *id2_head = m_head;
    pst_id2_tree *id2_ptr  = NULL;
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

    if (d_ptr->assoc_tree) {
        if (m_head) {
            DEBUG_WARN(("supplied master head, but have a list that is building a new id2_head\n"));
            m_head = NULL;
        }
        id2_head = pst_build_id2(pf, d_ptr->assoc_tree);
    }
    pst_printID2ptr(id2_head);

    list = pst_parse_block(pf, d_ptr->desc->i_id, id2_head);
    if (!list) {
        DEBUG_WARN(("pst_parse_block() returned an error for d_ptr->desc->i_id [%#"PRIx64"]\n", d_ptr->desc->i_id));
        if (!m_head) pst_free_id2(id2_head);
        DEBUG_RET();
        return NULL;
    }

    item = (pst_item*) pst_malloc(sizeof(pst_item));
    memset(item, 0, sizeof(pst_item));
    item->pf = pf;

    if (pst_process(d_ptr->desc->i_id, list, item, NULL)) {
        DEBUG_WARN(("pst_process() returned non-zero value. That is an error\n"));
        pst_freeItem(item);
        pst_free_list(list);
        if (!m_head) pst_free_id2(id2_head);
        DEBUG_RET();
        return NULL;
    }
    pst_free_list(list);

    if ((id2_ptr = pst_getID2(id2_head, (uint64_t)0x692))) {
        // DSN/MDN reports?
        DEBUG_INFO(("DSN/MDN processing\n"));
        list = pst_parse_block(pf, id2_ptr->id->i_id, id2_head);
        if (list) {
            for (x=0; x < list->count_objects; x++) {
                attach = (pst_item_attach*) pst_malloc(sizeof(pst_item_attach));
                memset(attach, 0, sizeof(pst_item_attach));
                attach->next = item->attach;
                item->attach = attach;
            }
            if (pst_process(id2_ptr->id->i_id, list, item, item->attach)) {
                DEBUG_WARN(("ERROR pst_process() failed with DSN/MDN attachments\n"));
                pst_freeItem(item);
                pst_free_list(list);
                if (!m_head) pst_free_id2(id2_head);
                DEBUG_RET();
                return NULL;
            }
            pst_free_list(list);
        } else {
            DEBUG_WARN(("ERROR error processing main DSN/MDN record\n"));
            // if (!m_head) pst_free_id2(id2_head);
            // DEBUG_RET();
            // return item;
        }
    }

    if ((id2_ptr = pst_getID2(id2_head, (uint64_t)0x671))) {
        DEBUG_INFO(("ATTACHMENT processing attachment\n"));
        list = pst_parse_block(pf, id2_ptr->id->i_id, id2_head);
        if (!list) {
            DEBUG_WARN(("ERROR error processing main attachment record\n"));
            if (!m_head) pst_free_id2(id2_head);
            DEBUG_RET();
            return item;
        }
        for (x=0; x < list->count_objects; x++) {
            attach = (pst_item_attach*) pst_malloc(sizeof(pst_item_attach));
            memset(attach, 0, sizeof(pst_item_attach));
            attach->next = item->attach;
            item->attach = attach;
        }
        if (pst_process(id2_ptr->id->i_id, list, item, item->attach)) {
            DEBUG_WARN(("ERROR pst_process() failed with attachments\n"));
            pst_freeItem(item);
            pst_free_list(list);
            if (!m_head) pst_free_id2(id2_head);
            DEBUG_RET();
            return NULL;
        }
        pst_free_list(list);

        // now we will have initial information of each attachment stored in item->attach...
        // we must now read the secondary record for each based on the id2_val associated with
        // each attachment
        for (attach = item->attach; attach; attach = attach->next) {
            DEBUG_WARN(("initial attachment id2 %#"PRIx64"\n", attach->id2_val));
            if ((id2_ptr = pst_getID2(id2_head, attach->id2_val))) {
                DEBUG_WARN(("initial attachment id2 found id %#"PRIx64"\n", id2_ptr->id->i_id));
                // id2_ptr is a record describing the attachment
                // we pass NULL instead of id2_head cause we don't want it to
                // load all the extra stuff here.
                list = pst_parse_block(pf, id2_ptr->id->i_id, NULL);
                if (!list) {
                    DEBUG_WARN(("ERROR error processing an attachment record\n"));
                    continue;
                }
                if (list->count_objects > 1) {
                    DEBUG_WARN(("ERROR probably fatal, list count array will overrun attach structure.\n"));
                }
                // reprocess the same attachment list against new data
                // this might update attach->id2_val
                if (pst_process(id2_ptr->id->i_id, list, item, attach)) {
                    DEBUG_WARN(("ERROR pst_process() failed with an attachment\n"));
                    pst_free_list(list);
                    continue;
                }
                pst_free_list(list);
                id2_ptr = pst_getID2(id2_head, attach->id2_val);
                if (id2_ptr) {
                    DEBUG_WARN(("second pass attachment updating id2 %#"PRIx64" found i_id %#"PRIx64"\n", attach->id2_val, id2_ptr->id->i_id));
                    // i_id has been updated to the datablock containing the attachment data
                    attach->i_id     = id2_ptr->id->i_id;
                    attach->id2_head = deep_copy(id2_ptr->child);
                } else {
                    DEBUG_WARN(("have not located the correct value for the attachment [%#"PRIx64"]\n", attach->id2_val));
                }
            } else {
                DEBUG_WARN(("ERROR cannot locate id2 value %#"PRIx64"\n", attach->id2_val));
                attach->id2_val = 0;    // suppress this missing attachment
            }
        }
    }

    if (!m_head) pst_free_id2(id2_head);
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


/** Process a low level descriptor block (0x0101, 0xbcec, 0x7cec) into a
 *  list of MAPI objects, each of which contains a list of MAPI elements.
 *
 *  @return list of MAPI objects
 */
static pst_mapi_object* pst_parse_block(pst_file *pf, uint64_t block_id, pst_id2_tree *i2_head) {
    pst_mapi_object *mo_head = NULL;
    char  *buf       = NULL;
    size_t read_size = 0;
    pst_subblocks  subblocks;
    pst_mapi_object *mo_ptr = NULL;
    pst_block_offset_pointer block_offset1;
    pst_block_offset_pointer block_offset2;
    pst_block_offset_pointer block_offset3;
    pst_block_offset_pointer block_offset4;
    pst_block_offset_pointer block_offset5;
    pst_block_offset_pointer block_offset6;
    pst_block_offset_pointer block_offset7;
    int32_t  x;
    int32_t  num_mapi_objects;
    int32_t  count_mapi_objects;
    int32_t  num_mapi_elements;
    int32_t  count_mapi_elements;
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
        DEBUG_WARN(("Error reading block id %#"PRIx64"\n", block_id));
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
    DEBUG_INFO(("block header (index_offset=%#hx, type=%#hx, offset=%#hx)\n", block_hdr.index_offset, block_hdr.type, block_hdr.offset));

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
        DEBUG_INFO(("block header (index_offset=%#hx, type=%#hx, offset=%#hx)\n", block_hdr.index_offset, block_hdr.type, block_hdr.offset));
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
            DEBUG_WARN(("internal error (bc.b5 offset %#x) in reading block id %#"PRIx64"\n", block_hdr.offset, block_id));
            freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
            DEBUG_RET();
            return NULL;
        }
        memcpy(&table_rec, block_offset1.from, sizeof(table_rec));
        LE16_CPU(table_rec.type);
        LE16_CPU(table_rec.ref_type);
        LE32_CPU(table_rec.value);
        DEBUG_INFO(("table_rec (type=%#hx, ref_type=%#hx, value=%#x)\n", table_rec.type, table_rec.ref_type, table_rec.value));

        if ((table_rec.type != (uint16_t)0x02B5) || (table_rec.ref_type != 6)) {
            DEBUG_WARN(("Unknown second block constant - %#hx %#hx for id %#"PRIx64"\n", table_rec.type, table_rec.ref_type, block_id));
            freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
            DEBUG_RET();
            return NULL;
        }

        if (pst_getBlockOffsetPointer(pf, i2_head, &subblocks, table_rec.value, &block_offset2)) {
            DEBUG_WARN(("internal error (bc.b5.desc offset #x) in reading block id %#"PRIx64"\n", table_rec.value, block_id));
            freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
            DEBUG_RET();
            return NULL;
        }
        list_start = block_offset2.from;
        to_ptr     = block_offset2.to;
        num_mapi_elements = (to_ptr - list_start)/sizeof(table_rec);
        num_mapi_objects  = 1; // only going to be one object in these blocks
    }
    else if (block_hdr.type == (uint16_t)0x7CEC) { //type 2
        block_type = 2;

        if (pst_getBlockOffsetPointer(pf, i2_head, &subblocks, block_hdr.offset, &block_offset3)) {
            DEBUG_WARN(("internal error (7c.7c offset %#x) in reading block id %#"PRIx64"\n", block_hdr.offset, block_id));
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
            DEBUG_WARN(("Error. There isn't a 7C where I want to see 7C!\n"));
            freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
            DEBUG_RET();
            return NULL;
        }

        rec_size = seven_c_blk.rec_size;
        num_mapi_elements = (int32_t)(unsigned)seven_c_blk.item_count;

        if (pst_getBlockOffsetPointer(pf, i2_head, &subblocks, seven_c_blk.b_five_offset, &block_offset4)) {
            DEBUG_WARN(("internal error (7c.b5 offset %#x) in reading block id %#"PRIx64"\n", seven_c_blk.b_five_offset, block_id));
            freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
            DEBUG_RET();
            return NULL;
        }
        memcpy(&table_rec, block_offset4.from, sizeof(table_rec));
        LE16_CPU(table_rec.type);
        LE16_CPU(table_rec.ref_type);
        LE32_CPU(table_rec.value);
        DEBUG_INFO(("table_rec (type=%#hx, ref_type=%#hx, value=%#x)\n", table_rec.type, table_rec.ref_type, table_rec.value));

        if (table_rec.type != (uint16_t)0x04B5) { // different constant than a type 1 record
            DEBUG_WARN(("Unknown second block constant - %#hx for id %#"PRIx64"\n", table_rec.type, block_id));
            freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
            DEBUG_RET();
            return NULL;
        }

        if (table_rec.value > 0) {
            if (pst_getBlockOffsetPointer(pf, i2_head, &subblocks, table_rec.value, &block_offset5)) {
                DEBUG_WARN(("internal error (7c.b5.desc offset %#x) in reading block id %#"PRIx64"\n", table_rec.value, block_id));
                freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
                DEBUG_RET();
                return NULL;
            }

            // this will give the number of records in this block
            num_mapi_objects = (block_offset5.to - block_offset5.from) / (4 + table_rec.ref_type);

            if (pst_getBlockOffsetPointer(pf, i2_head, &subblocks, seven_c_blk.ind2_offset, &block_offset6)) {
                DEBUG_WARN(("internal error (7c.ind2 offset %#x) in reading block id %#"PRIx64"\n", seven_c_blk.ind2_offset, block_id));
                freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
                DEBUG_RET();
                return NULL;
            }
            ind2_ptr = block_offset6.from;
            ind2_end = block_offset6.to;
        }
        else {
            num_mapi_objects = 0;
        }
        DEBUG_INFO(("7cec block index2 pointer %#x and end %#x\n", ind2_ptr, ind2_end));
    }
    else {
        DEBUG_WARN(("ERROR: Unknown block constant - %#hx for id %#"PRIx64"\n", block_hdr.type, block_id));
        freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
        DEBUG_RET();
        return NULL;
    }

    DEBUG_INFO(("found %i mapi objects each with %i mapi elements\n", num_mapi_objects, num_mapi_elements));
    for (count_mapi_objects=0; count_mapi_objects<num_mapi_objects; count_mapi_objects++) {
        // put another mapi object on the linked list
        mo_ptr = (pst_mapi_object*) pst_malloc(sizeof(pst_mapi_object));
        memset(mo_ptr, 0, sizeof(pst_mapi_object));
        mo_ptr->next = mo_head;
        mo_head = mo_ptr;
        // allocate the array of mapi elements
        mo_ptr->elements        = (pst_mapi_element**) pst_malloc(sizeof(pst_mapi_element)*num_mapi_elements);
        mo_ptr->count_elements  = num_mapi_elements;
        mo_ptr->orig_count      = num_mapi_elements;
        mo_ptr->count_objects   = (int32_t)num_mapi_objects; // each record will have a record of the total number of records
        for (x=0; x<num_mapi_elements; x++) mo_ptr->elements[x] = NULL;

        DEBUG_INFO(("going to read %i mapi elements for mapi object %i\n", num_mapi_elements, count_mapi_objects));

        fr_ptr = list_start;    // initialize fr_ptr to the start of the list.
        x = 0;                  // x almost tracks count_mapi_elements, but see 'continue' statement below
        for (count_mapi_elements=0; count_mapi_elements<num_mapi_elements; count_mapi_elements++) { //we will increase fr_ptr as we progress through index
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
                DEBUG_INFO(("reading element %i (type=%#x, ref_type=%#x, offset=%#x, size=%#x)\n",
                    x, table2_rec.type, table2_rec.ref_type, table2_rec.ind2_off, table2_rec.size));

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
                DEBUG_WARN(("Missing code for block_type %i\n", block_type));
                freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
                pst_free_list(mo_head);
                DEBUG_RET();
                return NULL;
            }
            DEBUG_INFO(("reading element %i (type=%#x, ref_type=%#x, value=%#x)\n",
                x, table_rec.type, table_rec.ref_type, table_rec.value));

            if (!mo_ptr->elements[x]) {
                mo_ptr->elements[x] = (pst_mapi_element*) pst_malloc(sizeof(pst_mapi_element));
            }
            memset(mo_ptr->elements[x], 0, sizeof(pst_mapi_element)); //init it

            // check here to see if the id of the attribute is a mapped one
            mapptr = pf->x_head;
            while (mapptr && (mapptr->map < table_rec.type)) mapptr = mapptr->next;
            if (mapptr && (mapptr->map == table_rec.type)) {
                if (mapptr->mytype == PST_MAP_ATTRIB) {
                    mo_ptr->elements[x]->mapi_id = *((uint32_t*)mapptr->data);
                    DEBUG_INFO(("Mapped attrib %#x to %#x\n", table_rec.type, mo_ptr->elements[x]->mapi_id));
                } else if (mapptr->mytype == PST_MAP_HEADER) {
                    DEBUG_INFO(("Internet Header mapping found %#"PRIx32" to %s\n", table_rec.type, mapptr->data));
                    mo_ptr->elements[x]->mapi_id = (uint32_t)PST_ATTRIB_HEADER;
                    mo_ptr->elements[x]->extra   = mapptr->data;
                }
                else {
                    DEBUG_WARN(("Missing assertion failure\n"));
                    // nothing, should be assertion failure here
                }
            } else {
                mo_ptr->elements[x]->mapi_id = table_rec.type;
            }
            mo_ptr->elements[x]->type = 0; // checked later before it is set
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
                mo_ptr->elements[x]->size = sizeof(int32_t);
                mo_ptr->elements[x]->type = table_rec.ref_type;
                mo_ptr->elements[x]->data = pst_malloc(sizeof(int32_t));
                memcpy(mo_ptr->elements[x]->data, &(table_rec.value), sizeof(int32_t));
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
                    mo_ptr->elements[x]->size = value_size;
                    mo_ptr->elements[x]->type = table_rec.ref_type;
                    mo_ptr->elements[x]->data = pst_malloc(value_size);
                    memcpy(mo_ptr->elements[x]->data, value_pointer, value_size);
                }
                else if (pst_getBlockOffsetPointer(pf, i2_head, &subblocks, table_rec.value, &block_offset7)) {
                    if ((table_rec.value & 0xf) == (uint32_t)0xf) {
                        DEBUG_WARN(("failed to get block offset for table_rec.value of %#x to be read later.\n", table_rec.value));
                        mo_ptr->elements[x]->size = 0;
                        mo_ptr->elements[x]->data = NULL;
                        mo_ptr->elements[x]->type = table_rec.value;
                    }
                    else {
                        if (table_rec.value) {
                            DEBUG_WARN(("failed to get block offset for table_rec.value of %#x\n", table_rec.value));
                        }
                        mo_ptr->count_elements --; //we will be skipping a row
                        continue;
                    }
                }
                else {
                    value_size = (size_t)(block_offset7.to - block_offset7.from);
                    mo_ptr->elements[x]->size = value_size;
                    mo_ptr->elements[x]->type = table_rec.ref_type;
                    mo_ptr->elements[x]->data = pst_malloc(value_size+1);
                    memcpy(mo_ptr->elements[x]->data, block_offset7.from, value_size);
                    mo_ptr->elements[x]->data[value_size] = '\0';  // it might be a string, null terminate it.
                }
                if (table_rec.ref_type == (uint16_t)0xd) {
                    // there is still more to do for the type of 0xD embedded objects
                    type_d_rec = (struct _type_d_rec*) mo_ptr->elements[x]->data;
                    LE32_CPU(type_d_rec->id);
                    mo_ptr->elements[x]->size = pst_ff_getID2block(pf, type_d_rec->id, i2_head, &(mo_ptr->elements[x]->data));
                    if (!mo_ptr->elements[x]->size){
                        DEBUG_WARN(("not able to read the ID2 data. Setting to be read later. %#x\n", type_d_rec->id));
                        mo_ptr->elements[x]->type = type_d_rec->id;    // fetch before freeing data, alias pointer
                        free(mo_ptr->elements[x]->data);
                        mo_ptr->elements[x]->data = NULL;
                    }
                }
                if (table_rec.ref_type == (uint16_t)0x1f) {
                    // there is more to do for the type 0x1f unicode strings
                    size_t rc;
                    static pst_vbuf *utf16buf = NULL;
                    static pst_vbuf *utf8buf  = NULL;
                    if (!utf16buf) utf16buf = pst_vballoc((size_t)1024);
                    if (!utf8buf)  utf8buf  = pst_vballoc((size_t)1024);

                    //need UTF-16 zero-termination
                    pst_vbset(utf16buf, mo_ptr->elements[x]->data, mo_ptr->elements[x]->size);
                    pst_vbappend(utf16buf, "\0\0", (size_t)2);
                    DEBUG_INFO(("Iconv in:\n"));
                    DEBUG_HEXDUMPC(utf16buf->b, utf16buf->dlen, 0x10);
                    rc = pst_vb_utf16to8(utf8buf, utf16buf->b, utf16buf->dlen);
                    if (rc == (size_t)-1) {
                        DEBUG_WARN(("Failed to convert utf-16 to utf-8\n"));
                    }
                    else {
                        free(mo_ptr->elements[x]->data);
                        mo_ptr->elements[x]->size = utf8buf->dlen;
                        mo_ptr->elements[x]->data = pst_malloc(utf8buf->dlen);
                        memcpy(mo_ptr->elements[x]->data, utf8buf->b, utf8buf->dlen);
                    }
                    DEBUG_INFO(("Iconv out:\n"));
                    DEBUG_HEXDUMPC(mo_ptr->elements[x]->data, mo_ptr->elements[x]->size, 0x10);
                }
                if (mo_ptr->elements[x]->type == 0) mo_ptr->elements[x]->type = table_rec.ref_type;
            } else {
                DEBUG_WARN(("ERROR Unknown ref_type %#hx\n", table_rec.ref_type));
                freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
                pst_free_list(mo_head);
                DEBUG_RET();
                return NULL;
            }
            x++;
        }
        DEBUG_INFO(("increasing ind2_ptr by %i [%#x] bytes. Was %#x, Now %#x\n", rec_size, rec_size, ind2_ptr, ind2_ptr+rec_size));
        ind2_ptr += rec_size;
    }
    freeall(&subblocks, &block_offset1, &block_offset2, &block_offset3, &block_offset4, &block_offset5, &block_offset6, &block_offset7);
    DEBUG_RET();
    return mo_head;
}


// This version of free does NULL check first
#define SAFE_FREE(x) {if (x) free(x);}
#define SAFE_FREE_STR(x) SAFE_FREE(x.str)
#define SAFE_FREE_BIN(x) SAFE_FREE(x.data)

// check if item->email is NULL, and init if so
#define MALLOC_EMAIL(x)        { if (!x->email)         { x->email         = (pst_item_email*)         pst_malloc(sizeof(pst_item_email));         memset(x->email,         0, sizeof(pst_item_email)        );} }
#define MALLOC_FOLDER(x)       { if (!x->folder)        { x->folder        = (pst_item_folder*)        pst_malloc(sizeof(pst_item_folder));        memset(x->folder,        0, sizeof(pst_item_folder)       );} }
#define MALLOC_CONTACT(x)      { if (!x->contact)       { x->contact       = (pst_item_contact*)       pst_malloc(sizeof(pst_item_contact));       memset(x->contact,       0, sizeof(pst_item_contact)      );} }
#define MALLOC_MESSAGESTORE(x) { if (!x->message_store) { x->message_store = (pst_item_message_store*) pst_malloc(sizeof(pst_item_message_store)); memset(x->message_store, 0, sizeof(pst_item_message_store));} }
#define MALLOC_JOURNAL(x)      { if (!x->journal)       { x->journal       = (pst_item_journal*)       pst_malloc(sizeof(pst_item_journal));       memset(x->journal,       0, sizeof(pst_item_journal)      );} }
#define MALLOC_APPOINTMENT(x)  { if (!x->appointment)   { x->appointment   = (pst_item_appointment*)   pst_malloc(sizeof(pst_item_appointment));   memset(x->appointment,   0, sizeof(pst_item_appointment)  );} }

// malloc space and copy the current item's data null terminated
#define LIST_COPY(targ, type) {                                    \
    targ = type pst_realloc(targ, list->elements[x]->size+1);      \
    memcpy(targ, list->elements[x]->data, list->elements[x]->size);\
    memset(((char*)targ)+list->elements[x]->size, 0, (size_t)1);   \
}

#define LIST_COPY_CSTR(targ) {                                              \
    if ((list->elements[x]->type == 0x1f) ||                                \
        (list->elements[x]->type == 0x1e) ||                                \
        (list->elements[x]->type == 0x102)) {                               \
        LIST_COPY(targ, (char*))                                            \
    }                                                                       \
    else {                                                                  \
        DEBUG_WARN(("src not 0x1e or 0x1f or 0x102 for string dst\n"));     \
        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);    \
        SAFE_FREE(targ);                                                    \
        targ = NULL;                                                        \
    }                                                                       \
}

#define LIST_COPY_BOOL(label, targ) {                                       \
    if (list->elements[x]->type != 0x0b) {                                  \
        DEBUG_WARN(("src not 0x0b for boolean dst\n"));                     \
        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);    \
    }                                                                       \
    if (*(int16_t*)list->elements[x]->data) {                               \
        DEBUG_INFO((label" - True\n"));                                     \
        targ = 1;                                                           \
    } else {                                                                \
        DEBUG_INFO((label" - False\n"));                                    \
        targ = 0;                                                           \
    }                                                                       \
}

#define LIST_COPY_EMAIL_BOOL(label, targ) {                     \
    MALLOC_EMAIL(item);                                         \
    LIST_COPY_BOOL(label, targ)                                 \
}

#define LIST_COPY_CONTACT_BOOL(label, targ) {                   \
    MALLOC_CONTACT(item);                                       \
    LIST_COPY_BOOL(label, targ)                                 \
}

#define LIST_COPY_APPT_BOOL(label, targ) {                      \
    MALLOC_APPOINTMENT(item);                                   \
    LIST_COPY_BOOL(label, targ)                                 \
}

#define LIST_COPY_INT16_N(targ) {                                           \
    if (list->elements[x]->type != 0x02) {                                  \
        DEBUG_WARN(("src not 0x02 for int16 dst\n"));                       \
        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);    \
    }                                                                       \
    memcpy(&(targ), list->elements[x]->data, sizeof(targ));                 \
    LE16_CPU(targ);                                                         \
}

#define LIST_COPY_INT16(label, targ) {                          \
    LIST_COPY_INT16_N(targ);                                    \
    DEBUG_INFO((label" - %i %#x\n", (int)targ, (int)targ));     \
}

#define LIST_COPY_INT32_N(targ) {                                           \
    if (list->elements[x]->type != 0x03) {                                  \
        DEBUG_WARN(("src not 0x03 for int32 dst\n"));                       \
        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);    \
    }                                                                       \
    memcpy(&(targ), list->elements[x]->data, sizeof(targ));                 \
    LE32_CPU(targ);                                                         \
}

#define LIST_COPY_INT32(label, targ) {                          \
    LIST_COPY_INT32_N(targ);                                    \
    DEBUG_INFO((label" - %i %#x\n", (int)targ, (int)targ));     \
}

#define LIST_COPY_EMAIL_INT32(label, targ) {                    \
    MALLOC_EMAIL(item);                                         \
    LIST_COPY_INT32(label, targ);                               \
}

#define LIST_COPY_APPT_INT32(label, targ) {                     \
    MALLOC_APPOINTMENT(item);                                   \
    LIST_COPY_INT32(label, targ);                               \
}

#define LIST_COPY_FOLDER_INT32(label, targ) {                   \
    MALLOC_FOLDER(item);                                        \
    LIST_COPY_INT32(label, targ);                               \
}

#define LIST_COPY_STORE_INT32(label, targ) {                    \
    MALLOC_MESSAGESTORE(item);                                  \
    LIST_COPY_INT32(label, targ);                               \
}

#define LIST_COPY_ENUM(label, targ, delta, count, ...) {        \
    char *tlabels[] = {__VA_ARGS__};                            \
    LIST_COPY_INT32_N(targ);                                    \
    targ += delta;                                              \
    DEBUG_INFO((label" - %s [%i]\n",                            \
        (((int)targ < 0) || ((int)targ >= count))               \
            ? "**invalid"                                       \
            : tlabels[(int)targ], (int)targ));                  \
}

#define LIST_COPY_EMAIL_ENUM(label, targ, delta, count, ...) {  \
    MALLOC_EMAIL(item);                                         \
    LIST_COPY_ENUM(label, targ, delta, count, __VA_ARGS__);     \
}

#define LIST_COPY_APPT_ENUM(label, targ, delta, count, ...) {   \
    MALLOC_APPOINTMENT(item);                                   \
    LIST_COPY_ENUM(label, targ, delta, count, __VA_ARGS__);     \
}

#define LIST_COPY_ENUM16(label, targ, delta, count, ...) {      \
    char *tlabels[] = {__VA_ARGS__};                            \
    LIST_COPY_INT16_N(targ);                                    \
    targ += delta;                                              \
    DEBUG_INFO((label" - %s [%i]\n",                            \
        (((int)targ < 0) || ((int)targ >= count))               \
            ? "**invalid"                                       \
            : tlabels[(int)targ], (int)targ));                  \
}

#define LIST_COPY_CONTACT_ENUM16(label, targ, delta, count, ...) {  \
    MALLOC_CONTACT(item);                                           \
    LIST_COPY_ENUM16(label, targ, delta, count, __VA_ARGS__);       \
}

#define LIST_COPY_ENTRYID(label, targ) {                        \
    LIST_COPY(targ, (pst_entryid*));                            \
    LE32_CPU(targ->u1);                                         \
    LE32_CPU(targ->id);                                         \
    DEBUG_INFO((label" u1=%#x, id=%#x\n", targ->u1, targ->id)); \
}

#define LIST_COPY_EMAIL_ENTRYID(label, targ) {                  \
    MALLOC_EMAIL(item);                                         \
    LIST_COPY_ENTRYID(label, targ);                             \
}

#define LIST_COPY_STORE_ENTRYID(label, targ) {                  \
    MALLOC_MESSAGESTORE(item);                                  \
    LIST_COPY_ENTRYID(label, targ);                             \
}


// malloc space and copy the current item's data null terminated
// including the utf8 flag
#define LIST_COPY_STR(label, targ) {                                    \
    LIST_COPY_CSTR(targ.str);                                           \
    targ.is_utf8 = (list->elements[x]->type == 0x1f) ? 1 : 0;           \
    DEBUG_INFO((label" - unicode %d - %s\n", targ.is_utf8, targ.str));  \
}

#define LIST_COPY_EMAIL_STR(label, targ) {                      \
    MALLOC_EMAIL(item);                                         \
    LIST_COPY_STR(label, targ);                                 \
}

#define LIST_COPY_CONTACT_STR(label, targ) {                    \
    MALLOC_CONTACT(item);                                       \
    LIST_COPY_STR(label, targ);                                 \
}

#define LIST_COPY_APPT_STR(label, targ) {                       \
    MALLOC_APPOINTMENT(item);                                   \
    LIST_COPY_STR(label, targ);                                 \
}

#define LIST_COPY_JOURNAL_STR(label, targ) {                    \
    MALLOC_JOURNAL(item);                                       \
    LIST_COPY_STR(label, targ);                                 \
}

// malloc space and copy the item filetime
#define LIST_COPY_TIME(label, targ) {                                       \
    if (list->elements[x]->type != 0x40) {                                  \
        DEBUG_WARN(("src not 0x40 for filetime dst\n"));                    \
        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);    \
    }                                                                       \
    targ = (FILETIME*) pst_realloc(targ, sizeof(FILETIME));                 \
    memcpy(targ, list->elements[x]->data, list->elements[x]->size);         \
    LE32_CPU(targ->dwLowDateTime);                                          \
    LE32_CPU(targ->dwHighDateTime);                                         \
    DEBUG_INFO((label" - %s", pst_fileTimeToAscii(targ, time_buffer)));     \
}

#define LIST_COPY_EMAIL_TIME(label, targ) {                     \
    MALLOC_EMAIL(item);                                         \
    LIST_COPY_TIME(label, targ);                                \
}

#define LIST_COPY_CONTACT_TIME(label, targ) {                   \
    MALLOC_CONTACT(item);                                       \
    LIST_COPY_TIME(label, targ);                                \
}

#define LIST_COPY_APPT_TIME(label, targ) {                      \
    MALLOC_APPOINTMENT(item);                                   \
    LIST_COPY_TIME(label, targ);                                \
}

#define LIST_COPY_JOURNAL_TIME(label, targ) {                   \
    MALLOC_JOURNAL(item);                                       \
    LIST_COPY_TIME(label, targ);                                \
}

// malloc space and copy the current item's data and size
#define LIST_COPY_BIN(targ) {                                       \
    targ.size = list->elements[x]->size;                            \
    if (targ.size) {                                                \
        targ.data = (char*)pst_realloc(targ.data, targ.size);       \
        memcpy(targ.data, list->elements[x]->data, targ.size);      \
    }                                                               \
    else {                                                          \
        SAFE_FREE_BIN(targ);                                        \
        targ.data = NULL;                                           \
    }                                                               \
}

#define LIST_COPY_EMAIL_BIN(label, targ) {          \
    MALLOC_EMAIL(item);                             \
    LIST_COPY_BIN(targ);                            \
    DEBUG_INFO((label"\n"));                        \
}
#define LIST_COPY_APPT_BIN(label, targ) {           \
    MALLOC_APPOINTMENT(item);                       \
    LIST_COPY_BIN(targ);                            \
    DEBUG_INFO((label"\n"));                        \
    DEBUG_HEXDUMP(targ.data, targ.size);            \
}

#define NULL_CHECK(x) { if (!x) { DEBUG_WARN(("NULL_CHECK: Null Found\n")); break;} }


/**
 * process the list of MAPI objects produced from parse_block()
 *
 * @param block_id  block number used by parse_block() to produce these MAPI objects
 * @param list  pointer to the list of MAPI objects from parse_block()
 * @param item  pointer to the high level item to be updated from the list.
 *              this item may be an email, contact or other sort of item.
 *              the type of this item is generally set by the MAPI elements
 *              from the list.
 * @param attach pointer to the list of attachment records. If
 *               this is non-null, the length of the this attachment list
 *               must be at least as large as the length of the MAPI objects list.
 *
 * @return 0 for ok, -1 for error.
 */
static int pst_process(uint64_t block_id, pst_mapi_object *list, pst_item *item, pst_item_attach *attach) {
    DEBUG_ENT("pst_process");
    if (!item) {
        DEBUG_WARN(("item cannot be NULL.\n"));
        DEBUG_RET();
        return -1;
    }

    item->block_id = block_id;
    while (list) {
        int32_t x;
        char time_buffer[30];
        for (x=0; x<list->count_elements; x++) {
            int32_t t;
            uint32_t ut;
            DEBUG_INFO(("#%d - mapi-id: %#x type: %#x length: %#x\n", x, list->elements[x]->mapi_id, list->elements[x]->type, list->elements[x]->size));

            switch (list->elements[x]->mapi_id) {
                case PST_ATTRIB_HEADER: // CUSTOM attribute for saying the Extra Headers
                    if (list->elements[x]->extra) {
                        if (list->elements[x]->type == 0x0101e) {
                            // an array of strings, rather than a single string
                            int32_t string_length, i, offset, next_offset;
                            int32_t p = 0;
                            int32_t array_element_count = PST_LE_GET_INT32(list->elements[x]->data); p+=4;
                            for (i = 1; i <= array_element_count; i++) {
                                pst_item_extra_field *ef = (pst_item_extra_field*) pst_malloc(sizeof(pst_item_extra_field));
                                memset(ef, 0, sizeof(pst_item_extra_field));
                                offset      = PST_LE_GET_INT32(list->elements[x]->data + p); p+=4;
                                next_offset = (i == array_element_count) ? list->elements[x]->size : PST_LE_GET_INT32(list->elements[x]->data + p);;
                                string_length = next_offset - offset;
                                ef->value = pst_malloc(string_length + 1);
                                memcpy(ef->value, list->elements[x]->data + offset, string_length);
                                ef->value[string_length] = '\0';
                                ef->field_name = strdup(list->elements[x]->extra);
                                ef->next       = item->extra_fields;
                                item->extra_fields = ef;
                                DEBUG_INFO(("Extra Field - \"%s\" = \"%s\"\n", ef->field_name, ef->value));
                            }
                        }
                        else {
                            // should be a single string
                            pst_item_extra_field *ef = (pst_item_extra_field*) pst_malloc(sizeof(pst_item_extra_field));
                            memset(ef, 0, sizeof(pst_item_extra_field));
                            LIST_COPY_CSTR(ef->value);
                            if (ef->value) {
                                ef->field_name = strdup(list->elements[x]->extra);
                                ef->next       = item->extra_fields;
                                item->extra_fields = ef;
                                DEBUG_INFO(("Extra Field - \"%s\" = \"%s\"\n", ef->field_name, ef->value));
                                if (strcmp(ef->field_name, "content-type") == 0) {
                                    char *p = strstr(ef->value, "charset=\"");
                                    if (p) {
                                        p += 9; // skip over charset="
                                        char *pp = strchr(p, '"');
                                        if (pp) {
                                            *pp = '\0';
                                            char *set = strdup(p);
                                            *pp = '"';
                                            if (item->body_charset.str) free(item->body_charset.str);
                                            item->body_charset.str     = set;
                                            item->body_charset.is_utf8 = 1;
                                            DEBUG_INFO(("body charset %s from content-type extra field\n", set));
                                        }
                                    }
                                }
                            }
                            else {
                                DEBUG_WARN(("What does this mean? Internet header %s value\n", list->elements[x]->extra));
                                DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);
                                free(ef);   // caught by valgrind
                            }
                        }
                    }
                    break;
                case 0x0002: // PR_ALTERNATE_RECIPIENT_ALLOWED
                    if (list->elements[x]->type == 0x0b) {
                        // If set to true, the sender allows this email to be autoforwarded
                        LIST_COPY_EMAIL_BOOL("AutoForward allowed", item->email->autoforward);
                        if (!item->email->autoforward) item->email->autoforward = -1;
                    } else {
                        DEBUG_WARN(("What does this mean?\n"));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);
                    }
                    break;
                case 0x0003: // Extended Attributes table
                    DEBUG_INFO(("Extended Attributes Table - NOT PROCESSED\n"));
                    break;
                case 0x0017: // PR_IMPORTANCE - How important the sender deems it to be
                    LIST_COPY_EMAIL_ENUM("Importance Level", item->email->importance, 0, 3, "Low", "Normal", "High");
                    break;
                case 0x001A: // PR_MESSAGE_CLASS IPM.x
                    if ((list->elements[x]->type == 0x1e) ||
                        (list->elements[x]->type == 0x1f)) {
                        LIST_COPY_CSTR(item->ascii_type);
                        if (!item->ascii_type) item->ascii_type = strdup("unknown");
                        if (pst_strincmp("IPM.Note", item->ascii_type, 8) == 0)
                            item->type = PST_TYPE_NOTE;
                        else if (pst_stricmp("IPM", item->ascii_type) == 0)
                            item->type = PST_TYPE_NOTE;
                        else if (pst_strincmp("IPM.Contact", item->ascii_type, 11) == 0)
                            item->type = PST_TYPE_CONTACT;
                        else if (pst_strincmp("REPORT.IPM.Note", item->ascii_type, 15) == 0)
                            item->type = PST_TYPE_REPORT;
                        else if (pst_strincmp("IPM.Activity", item->ascii_type, 12) == 0)
                            item->type = PST_TYPE_JOURNAL;
                        else if (pst_strincmp("IPM.Appointment", item->ascii_type, 15) == 0)
                            item->type = PST_TYPE_APPOINTMENT;
                        else if (pst_strincmp("IPM.Schedule.Meeting", item->ascii_type, 20) == 0)
                            item->type = PST_TYPE_SCHEDULE;     // meeting requests and responses transported over email
                        else if (pst_strincmp("IPM.StickyNote", item->ascii_type, 14) == 0)
                            item->type = PST_TYPE_STICKYNOTE;
                        else if (pst_strincmp("IPM.Task", item->ascii_type, 8) == 0)
                            item->type = PST_TYPE_TASK;
                        else
                            item->type = PST_TYPE_OTHER;
                        DEBUG_INFO(("Message class %s [%"PRIi32"] \n", item->ascii_type, item->type));
                    }
                    else {
                        DEBUG_WARN(("What does this mean?\n"));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);
                    }
                    break;
                case 0x0023: // PR_ORIGINATOR_DELIVERY_REPORT_REQUESTED
                    if (list->elements[x]->type == 0x0b) {
                        // set if the sender wants a delivery report from all recipients
                        LIST_COPY_EMAIL_BOOL("Global Delivery Report", item->email->delivery_report);
                    }
                    else {
                        DEBUG_WARN(("What does this mean?\n"));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);
                    }
                    break;
                case 0x0026: // PR_PRIORITY
                    LIST_COPY_EMAIL_ENUM("Priority", item->email->priority, 1, 3, "NonUrgent", "Normal", "Urgent");
                    break;
                case 0x0029: // PR_READ_RECEIPT_REQUESTED
                    LIST_COPY_EMAIL_BOOL("Read Receipt", item->email->read_receipt);
                    break;
                case 0x002B: // PR_RECIPIENT_REASSIGNMENT_PROHIBITED
                    LIST_COPY_BOOL("Reassignment Prohibited (Private)", item->private_member);
                    break;
                case 0x002E: // PR_ORIGINAL_SENSITIVITY - the sensitivity of the message before being replied to or forwarded
                    LIST_COPY_EMAIL_ENUM("Original Sensitivity", item->email->original_sensitivity, 0, 4,
                        "None", "Personal", "Private", "Company Confidential");
                    break;
                case 0x0032: // PR_REPORT_TIME
                    LIST_COPY_EMAIL_TIME("Report time", item->email->report_time);
                    break;
                case 0x0036: // PR_SENSITIVITY - sender's opinion of the sensitivity of an email
                    LIST_COPY_EMAIL_ENUM("Sensitivity", item->email->sensitivity, 0, 4,
                        "None", "Personal", "Private", "Company Confidential");
                    break;
                case 0x0037: // PR_SUBJECT raw subject
                    {
                        int off = 0;
                        if ((list->elements[x]->size > 2) && (((uint8_t)list->elements[x]->data[0]) < 0x20)) {
                            off = 2;
                        }
                        list->elements[x]->data += off;
                        list->elements[x]->size -= off;
                        LIST_COPY_STR("Raw Subject", item->subject);
                        list->elements[x]->size += off;
                        list->elements[x]->data -= off;
                    }
                    break;
                case 0x0039: // PR_CLIENT_SUBMIT_TIME Date Email Sent/Created
                    LIST_COPY_EMAIL_TIME("Date sent", item->email->sent_date);
                    break;
                case 0x003B: // PR_SENT_REPRESENTING_SEARCH_KEY Sender address 1
                    LIST_COPY_EMAIL_STR("Sent on behalf of address 1", item->email->outlook_sender);
                    break;
                case 0x003F: // PR_RECEIVED_BY_ENTRYID Structure containing Recipient
                    DEBUG_INFO(("Recipient Structure 1 -- NOT PROCESSED\n"));
                    break;
                case 0x0040: // PR_RECEIVED_BY_NAME Name of Recipient Structure
                    LIST_COPY_EMAIL_STR("Received By Name 1", item->email->outlook_received_name1);
                    break;
                case 0x0041: // PR_SENT_REPRESENTING_ENTRYID Structure containing Sender
                    DEBUG_INFO(("Sent on behalf of Structure 1 -- NOT PROCESSED\n"));
                    break;
                case 0x0042: // PR_SENT_REPRESENTING_NAME
                    LIST_COPY_EMAIL_STR("Sent on behalf of", item->email->outlook_sender_name);
                    break;
                case 0x0043: // PR_RCVD_REPRESENTING_ENTRYID Recipient Structure 2
                    DEBUG_INFO(("Received on behalf of Structure -- NOT PROCESSED\n"));
                    break;
                case 0x0044: // PR_RCVD_REPRESENTING_NAME
                    LIST_COPY_EMAIL_STR("Received on behalf of", item->email->outlook_recipient_name);
                    break;
                case 0x004F: // PR_REPLY_RECIPIENT_ENTRIES Reply-To Structure
                    DEBUG_INFO(("Reply-To Structure -- NOT PROCESSED\n"));
                    break;
                case 0x0050: // PR_REPLY_RECIPIENT_NAMES Name of Reply-To Structure
                    LIST_COPY_EMAIL_STR("Reply-To", item->email->reply_to);
                    break;
                case 0x0051: // PR_RECEIVED_BY_SEARCH_KEY Recipient Address 1
                    LIST_COPY_EMAIL_STR("Recipient's Address 1", item->email->outlook_recipient);
                    break;
                case 0x0052: // PR_RCVD_REPRESENTING_SEARCH_KEY Recipient Address 2
                    LIST_COPY_EMAIL_STR("Recipient's Address 2", item->email->outlook_recipient2);
                    break;
                case 0x0057: // PR_MESSAGE_TO_ME
                    // this user is listed explicitly in the TO address
                    LIST_COPY_EMAIL_BOOL("My address in TO field", item->email->message_to_me);
                    break;
                case 0x0058: // PR_MESSAGE_CC_ME
                    // this user is listed explicitly in the CC address
                    LIST_COPY_EMAIL_BOOL("My address in CC field", item->email->message_cc_me);
                    break;
                case 0x0059: // PR_MESSAGE_RECIP_ME
                    // this user appears in TO, CC or BCC address list
                    LIST_COPY_EMAIL_BOOL("Message addressed to me", item->email->message_recip_me);
                    break;
                case 0x0063: // PR_RESPONSE_REQUESTED
                    LIST_COPY_BOOL("Response requested", item->response_requested);
                    break;
                case 0x0064: // PR_SENT_REPRESENTING_ADDRTYPE Access method for Sender Address
                    LIST_COPY_EMAIL_STR("Sent on behalf of address type", item->email->sender_access);
                    break;
                case 0x0065: // PR_SENT_REPRESENTING_EMAIL_ADDRESS Sender Address
                    LIST_COPY_EMAIL_STR("Sent on behalf of address", item->email->sender_address);
                    break;
                case 0x0070: // PR_CONVERSATION_TOPIC Processed Subject
                    LIST_COPY_EMAIL_STR("Processed Subject (Conversation Topic)", item->email->processed_subject);
                    break;
                case 0x0071: // PR_CONVERSATION_INDEX
                    LIST_COPY_EMAIL_BIN("Conversation Index", item->email->conversation_index);
                    break;
                case 0x0072: // PR_ORIGINAL_DISPLAY_BCC
                    LIST_COPY_EMAIL_STR("Original display bcc", item->email->original_bcc);
                    break;
                case 0x0073: // PR_ORIGINAL_DISPLAY_CC
                    LIST_COPY_EMAIL_STR("Original display cc", item->email->original_cc);
                    break;
                case 0x0074: // PR_ORIGINAL_DISPLAY_TO
                    LIST_COPY_EMAIL_STR("Original display to", item->email->original_to);
                    break;
                case 0x0075: // PR_RECEIVED_BY_ADDRTYPE Recipient Access Method
                    LIST_COPY_EMAIL_STR("Received by Address type", item->email->recip_access);
                    break;
                case 0x0076: // PR_RECEIVED_BY_EMAIL_ADDRESS Recipient Address
                    LIST_COPY_EMAIL_STR("Received by Address", item->email->recip_address);
                    break;
                case 0x0077: // PR_RCVD_REPRESENTING_ADDRTYPE Recipient Access Method 2
                    LIST_COPY_EMAIL_STR("Received on behalf of Address type", item->email->recip2_access);
                    break;
                case 0x0078: // PR_RCVD_REPRESENTING_EMAIL_ADDRESS Recipient Address 2
                    LIST_COPY_EMAIL_STR("Received on behalf of Address", item->email->recip2_address);
                    break;
                case 0x007D: // PR_TRANSPORT_MESSAGE_HEADERS Internet Header
                    LIST_COPY_EMAIL_STR("Internet Header", item->email->header);
                    break;
                case 0x0C04: // PR_NDR_REASON_CODE
                    LIST_COPY_EMAIL_INT32("NDR reason code", item->email->ndr_reason_code);
                    break;
                case 0x0C05: // PR_NDR_DIAG_CODE
                    LIST_COPY_EMAIL_INT32("NDR diag code", item->email->ndr_diag_code);
                    break;
                case 0x0C06: // PR_NON_RECEIPT_NOTIFICATION_REQUESTED
                    DEBUG_INFO(("Non-Receipt Notification Requested -- NOT PROCESSED\n"));
                    break;
                case 0x0C17: // PR_REPLY_REQUESTED
                    LIST_COPY_EMAIL_BOOL("Reply Requested", item->email->reply_requested);
                    break;
                case 0x0C19: // PR_SENDER_ENTRYID Sender Structure 2
                    DEBUG_INFO(("Sender Structure 2 -- NOT PROCESSED\n"));
                    break;
                case 0x0C1A: // PR_SENDER_NAME Name of Sender Structure 2
                    LIST_COPY_EMAIL_STR("Name of Sender Structure 2", item->email->outlook_sender_name2);
                    break;
                case 0x0C1B: // PR_SUPPLEMENTARY_INFO
                    LIST_COPY_EMAIL_STR("Supplementary info", item->email->supplementary_info);
                    break;
                case 0x0C1D: // PR_SENDER_SEARCH_KEY Name of Sender Address 2
                    LIST_COPY_EMAIL_STR("Name of Sender Address 2 (Sender search key)", item->email->outlook_sender2);
                    break;
                case 0x0C1E: // PR_SENDER_ADDRTYPE Sender Address 2 access method
                    LIST_COPY_EMAIL_STR("Sender Address type", item->email->sender2_access);
                    break;
                case 0x0C1F: // PR_SENDER_EMAIL_ADDRESS Sender Address 2
                    LIST_COPY_EMAIL_STR("Sender Address", item->email->sender2_address);
                    break;
                case 0x0C20: // PR_NDR_STATUS_CODE
                    LIST_COPY_EMAIL_INT32("NDR status code", item->email->ndr_status_code);
                    break;
                case 0x0E01: // PR_DELETE_AFTER_SUBMIT
                    LIST_COPY_EMAIL_BOOL("Delete after submit", item->email->delete_after_submit);
                    break;
                case 0x0E02: // PR_DISPLAY_BCC BCC Addresses
                    LIST_COPY_EMAIL_STR("Display BCC Addresses", item->email->bcc_address);
                    break;
                case 0x0E03: // PR_DISPLAY_CC CC Addresses
                    LIST_COPY_EMAIL_STR("Display CC Addresses", item->email->cc_address);
                    break;
                case 0x0E04: // PR_DISPLAY_TO Address Sent-To
                    LIST_COPY_EMAIL_STR("Display Sent-To Address", item->email->sentto_address);
                    break;
                case 0x0E06: // PR_MESSAGE_DELIVERY_TIME Date 3 - Email Arrival Date
                    LIST_COPY_EMAIL_TIME("Date 3 (Delivery Time)", item->email->arrival_date);
                    break;
                case 0x0E07: // PR_MESSAGE_FLAGS Email Flag
                    LIST_COPY_EMAIL_INT32("Message Flags", item->flags);
                    break;
                case 0x0E08: // PR_MESSAGE_SIZE Total size of a message object
                    LIST_COPY_INT32("Message Size", item->message_size);
                    break;
                case 0x0E0A: // PR_SENTMAIL_ENTRYID
                    // folder that this message is sent to after submission
                    LIST_COPY_EMAIL_ENTRYID("Sentmail EntryID", item->email->sentmail_folder);
                    break;
                case 0x0E1D: // PR_NORMALIZED_SUBJECT
                    LIST_COPY_EMAIL_STR("Normalized subject", item->email->outlook_normalized_subject);
                    break;
                case 0x0E1F: // PR_RTF_IN_SYNC
                    // True means that the rtf version is same as text body
                    // False means rtf version is more up-to-date than text body
                    // if this value doesn't exist, text body is more up-to-date than rtf and
                    // cannot update to the rtf
                    LIST_COPY_EMAIL_BOOL("Compressed RTF in Sync", item->email->rtf_in_sync);
                    break;
                case 0x0E20: // PR_ATTACH_SIZE binary Attachment data in record
                    NULL_CHECK(attach);
                    LIST_COPY_INT32("Attachment Size", t);
                    // ignore this. we either get data and size from 0x3701
                    // or id codes from 0x3701 or 0x67f2
                    break;
                case 0x0FF9: // PR_RECORD_KEY Record Header 1
                    LIST_COPY_BIN(item->record_key);
                    DEBUG_INFO(("Record Key\n"));
                    DEBUG_HEXDUMP(item->record_key.data, item->record_key.size);
                    break;
                case 0x1000: // PR_BODY
                    LIST_COPY_STR("Plain Text body", item->body);
                    break;
                case 0x1001: // PR_REPORT_TEXT
                    LIST_COPY_EMAIL_STR("Report Text", item->email->report_text);
                    break;
                case 0x1006: // PR_RTF_SYNC_BODY_CRC
                    LIST_COPY_EMAIL_INT32("RTF Sync Body CRC", item->email->rtf_body_crc);
                    break;
                case 0x1007: // PR_RTF_SYNC_BODY_COUNT
                    // a count of the *significant* charcters in the rtf body. Doesn't count
                    // whitespace and other ignorable characters
                    LIST_COPY_EMAIL_INT32("RTF Sync Body character count", item->email->rtf_body_char_count);
                    break;
                case 0x1008: // PR_RTF_SYNC_BODY_TAG
                    // the first couple of lines of RTF body so that after modification, then beginning can
                    // once again be found
                    LIST_COPY_EMAIL_STR("RTF Sync body tag", item->email->rtf_body_tag);
                    break;
                case 0x1009: // PR_RTF_COMPRESSED - rtf data is lzw compressed
                    LIST_COPY_EMAIL_BIN("RTF Compressed body", item->email->rtf_compressed);
                    break;
                case 0x1010: // PR_RTF_SYNC_PREFIX_COUNT
                    // a count of the ignored characters before the first significant character
                    LIST_COPY_EMAIL_INT32("RTF whitespace prefix count", item->email->rtf_ws_prefix_count);
                    break;
                case 0x1011: // PR_RTF_SYNC_TRAILING_COUNT
                    // a count of the ignored characters after the last significant character
                    LIST_COPY_EMAIL_INT32("RTF whitespace tailing count", item->email->rtf_ws_trailing_count);
                    break;
                case 0x1013: // HTML body
                    LIST_COPY_EMAIL_STR("HTML body", item->email->htmlbody);
                    break;
                case 0x1035: // Message ID
                    LIST_COPY_EMAIL_STR("Message ID", item->email->messageid);
                    break;
                case 0x1042: // in-reply-to
                    LIST_COPY_EMAIL_STR("In-Reply-To", item->email->in_reply_to);
                    break;
                case 0x1046: // Return Path - this seems to be the message-id of the rfc822 mail that is being returned
                    LIST_COPY_EMAIL_STR("Return Path", item->email->return_path_address);
                    break;
                case 0x3001: // PR_DISPLAY_NAME File As
                    LIST_COPY_STR("Display Name", item->file_as);
                    break;
                case 0x3002: // PR_ADDRTYPE
                    LIST_COPY_CONTACT_STR("Address Type", item->contact->address1_transport);
                    break;
                case 0x3003: // PR_EMAIL_ADDRESS
                    LIST_COPY_CONTACT_STR("Contact email Address", item->contact->address1);
                    break;
                case 0x3004: // PR_COMMENT Comment for item - usually folders
                    LIST_COPY_STR("Comment", item->comment);
                    break;
                case 0x3007: // PR_CREATION_TIME Date 4 - Creation Date?
                    LIST_COPY_TIME("Date 4 (Item Creation Date)", item->create_date);
                    break;
                case 0x3008: // PR_LAST_MODIFICATION_TIME Date 5 - Modify Date
                    LIST_COPY_TIME("Date 5 (Modify Date)", item->modify_date);
                    break;
                case 0x300B: // PR_SEARCH_KEY Record Header 2
                    LIST_COPY_EMAIL_STR("Record Search 2", item->email->outlook_search_key);
                    break;
                case 0x35DF: // PR_VALID_FOLDER_MASK
                    LIST_COPY_STORE_INT32("Valid Folder Mask", item->message_store->valid_mask);
                    break;
                case 0x35E0: // PR_IPM_SUBTREE_ENTRYID Top of Personal Folder Record
                    LIST_COPY_STORE_ENTRYID("Top of Personal Folder Record", item->message_store->top_of_personal_folder);
                    break;
                case 0x35E2: // PR_IPM_OUTBOX_ENTRYID
                    LIST_COPY_STORE_ENTRYID("Default Outbox Folder record", item->message_store->default_outbox_folder);
                    break;
                case 0x35E3: // PR_IPM_WASTEBASKET_ENTRYID
                    LIST_COPY_STORE_ENTRYID("Deleted Items Folder record", item->message_store->deleted_items_folder);
                    break;
                case 0x35E4: // PR_IPM_SENTMAIL_ENTRYID
                    LIST_COPY_STORE_ENTRYID("Sent Items Folder record", item->message_store->sent_items_folder);
                    break;
                case 0x35E5: // PR_VIEWS_ENTRYID
                    LIST_COPY_STORE_ENTRYID("User Views Folder record", item->message_store->user_views_folder);
                    break;
                case 0x35E6: // PR_COMMON_VIEWS_ENTRYID
                    LIST_COPY_STORE_ENTRYID("Common View Folder record", item->message_store->common_view_folder);
                    break;
                case 0x35E7: // PR_FINDER_ENTRYID
                    LIST_COPY_STORE_ENTRYID("Search Root Folder record", item->message_store->search_root_folder);
                    break;
                case 0x3602: // PR_CONTENT_COUNT Number of emails stored in a folder
                    LIST_COPY_FOLDER_INT32("Folder Email Count", item->folder->item_count);
                    break;
                case 0x3603: // PR_CONTENT_UNREAD Number of unread emails
                    LIST_COPY_FOLDER_INT32("Unread Email Count", item->folder->unseen_item_count);
                    break;
                case 0x360A: // PR_SUBFOLDERS Has children
                    MALLOC_FOLDER(item);
                    LIST_COPY_BOOL("Has Subfolders", item->folder->subfolder);
                    break;
                case 0x3613: // PR_CONTAINER_CLASS IPF.x
                    LIST_COPY_CSTR(item->ascii_type);
                    if (pst_strincmp("IPF.Note", item->ascii_type, 8) == 0)
                        item->type = PST_TYPE_NOTE;
                    else if (pst_strincmp("IPF.Imap", item->ascii_type, 8) == 0)
                        item->type = PST_TYPE_NOTE;
                    else if (pst_stricmp("IPF", item->ascii_type) == 0)
                        item->type = PST_TYPE_NOTE;
                    else if (pst_strincmp("IPF.Contact", item->ascii_type, 11) == 0)
                        item->type = PST_TYPE_CONTACT;
                    else if (pst_strincmp("IPF.Journal", item->ascii_type, 11) == 0)
                        item->type = PST_TYPE_JOURNAL;
                    else if (pst_strincmp("IPF.Appointment", item->ascii_type, 15) == 0)
                        item->type = PST_TYPE_APPOINTMENT;
                    else if (pst_strincmp("IPF.StickyNote", item->ascii_type, 14) == 0)
                        item->type = PST_TYPE_STICKYNOTE;
                    else if (pst_strincmp("IPF.Task", item->ascii_type, 8) == 0)
                        item->type = PST_TYPE_TASK;
                    else
                        item->type = PST_TYPE_OTHER;

                    DEBUG_INFO(("Container class %s [%"PRIi32"]\n", item->ascii_type, item->type));
                    break;
                case 0x3617: // PR_ASSOC_CONTENT_COUNT
                    // associated content are items that are attached to this folder
                    // but are hidden from users
                    LIST_COPY_FOLDER_INT32("Associated Content count", item->folder->assoc_count);
                    break;
                case 0x3701: // PR_ATTACH_DATA_OBJ binary data of attachment
                    DEBUG_INFO(("Binary Data [Size %i]\n", list->elements[x]->size));
                    NULL_CHECK(attach);
                    if (!list->elements[x]->data) { //special case
                        attach->id2_val = list->elements[x]->type;
                        DEBUG_INFO(("Seen a Reference. The data hasn't been loaded yet. [%#"PRIx64"]\n", attach->id2_val));
                    } else {
                        LIST_COPY_BIN(attach->data);
                    }
                    break;
                case 0x3704: // PR_ATTACH_FILENAME Attachment filename (8.3)
                    NULL_CHECK(attach);
                    LIST_COPY_STR("Attachment Filename", attach->filename1);
                    break;
                case 0x3705: // PR_ATTACH_METHOD
                    NULL_CHECK(attach);
                    LIST_COPY_ENUM("Attachment method", attach->method, 0, 7,
                        "No Attachment",
                        "Attach By Value",
                        "Attach By Reference",
                        "Attach by Reference Resolve",
                        "Attach by Reference Only",
                        "Embedded Message",
                        "OLE");
                    break;
                case 0x3707: // PR_ATTACH_LONG_FILENAME Attachment filename (long?)
                    NULL_CHECK(attach);
                    LIST_COPY_STR("Attachment Filename long", attach->filename2);
                    break;
                case 0x370B: // PR_RENDERING_POSITION
                    // position in characters that the attachment appears in the plain text body
                    NULL_CHECK(attach);
                    LIST_COPY_INT32("Attachment Position", attach->position);
                    break;
                case 0x370E: // PR_ATTACH_MIME_TAG Mime type of encoding
                    NULL_CHECK(attach);
                    LIST_COPY_STR("Attachment mime encoding", attach->mimetype);
                    break;
                case 0x3710: // PR_ATTACH_MIME_SEQUENCE
                    // sequence number for mime parts. Includes body
                    NULL_CHECK(attach);
                    LIST_COPY_INT32("Attachment Mime Sequence", attach->sequence);
                    break;
                case 0x3A00: // PR_ACCOUNT
                    LIST_COPY_CONTACT_STR("Contact's Account name", item->contact->account_name);
                    break;
                case 0x3A01: // PR_ALTERNATE_RECIPIENT
                    DEBUG_INFO(("Contact Alternate Recipient - NOT PROCESSED\n"));
                    break;
                case 0x3A02: // PR_CALLBACK_TELEPHONE_NUMBER
                    LIST_COPY_CONTACT_STR("Callback telephone number", item->contact->callback_phone);
                    break;
                case 0x3A03: // PR_CONVERSION_PROHIBITED
                    LIST_COPY_EMAIL_BOOL("Message Conversion Prohibited", item->email->conversion_prohibited);
                    break;
                case 0x3A05: // PR_GENERATION suffix
                    LIST_COPY_CONTACT_STR("Contacts Suffix", item->contact->suffix);
                    break;
                case 0x3A06: // PR_GIVEN_NAME Contact's first name
                    LIST_COPY_CONTACT_STR("Contacts First Name", item->contact->first_name);
                    break;
                case 0x3A07: // PR_GOVERNMENT_ID_NUMBER
                    LIST_COPY_CONTACT_STR("Contacts Government ID Number", item->contact->gov_id);
                    break;
                case 0x3A08: // PR_BUSINESS_TELEPHONE_NUMBER
                    LIST_COPY_CONTACT_STR("Business Telephone Number", item->contact->business_phone);
                    break;
                case 0x3A09: // PR_HOME_TELEPHONE_NUMBER
                    LIST_COPY_CONTACT_STR("Home Telephone Number", item->contact->home_phone);
                    break;
                case 0x3A0A: // PR_INITIALS Contact's Initials
                    LIST_COPY_CONTACT_STR("Contacts Initials", item->contact->initials);
                    break;
                case 0x3A0B: // PR_KEYWORD
                    LIST_COPY_CONTACT_STR("Keyword", item->contact->keyword);
                    break;
                case 0x3A0C: // PR_LANGUAGE
                    LIST_COPY_CONTACT_STR("Contact's Language", item->contact->language);
                    break;
                case 0x3A0D: // PR_LOCATION
                    LIST_COPY_CONTACT_STR("Contact's Location", item->contact->location);
                    break;
                case 0x3A0E: // PR_MAIL_PERMISSION - Can the recipient receive and send email
                    LIST_COPY_CONTACT_BOOL("Mail Permission", item->contact->mail_permission);
                    break;
                case 0x3A0F: // PR_MHS_COMMON_NAME
                    LIST_COPY_CONTACT_STR("MHS Common Name", item->contact->common_name);
                    break;
                case 0x3A10: // PR_ORGANIZATIONAL_ID_NUMBER
                    LIST_COPY_CONTACT_STR("Organizational ID #", item->contact->org_id);
                    break;
                case 0x3A11: // PR_SURNAME Contact's Surname
                    LIST_COPY_CONTACT_STR("Contacts Surname", item->contact->surname);
                    break;
                case 0x3A12: // PR_ORIGINAL_ENTRY_ID
                    DEBUG_INFO(("Original Entry ID - NOT PROCESSED\n"));
                    break;
                case 0x3A13: // PR_ORIGINAL_DISPLAY_NAME
                    DEBUG_INFO(("Original Display Name - NOT PROCESSED\n"));
                    break;
                case 0x3A14: // PR_ORIGINAL_SEARCH_KEY
                    DEBUG_INFO(("Original Search Key - NOT PROCESSED\n"));
                    break;
                case 0x3A15: // PR_POSTAL_ADDRESS
                    LIST_COPY_CONTACT_STR("Default Postal Address", item->contact->def_postal_address);
                    break;
                case 0x3A16: // PR_COMPANY_NAME
                    LIST_COPY_CONTACT_STR("Company Name", item->contact->company_name);
                    break;
                case 0x3A17: // PR_TITLE - Job Title
                    LIST_COPY_CONTACT_STR("Job Title", item->contact->job_title);
                    break;
                case 0x3A18: // PR_DEPARTMENT_NAME
                    LIST_COPY_CONTACT_STR("Department Name", item->contact->department);
                    break;
                case 0x3A19: // PR_OFFICE_LOCATION
                    LIST_COPY_CONTACT_STR("Office Location", item->contact->office_loc);
                    break;
                case 0x3A1A: // PR_PRIMARY_TELEPHONE_NUMBER
                    LIST_COPY_CONTACT_STR("Primary Telephone", item->contact->primary_phone);
                    break;
                case 0x3A1B: // PR_BUSINESS2_TELEPHONE_NUMBER
                    LIST_COPY_CONTACT_STR("Business Phone Number 2", item->contact->business_phone2);
                    break;
                case 0x3A1C: // PR_MOBILE_TELEPHONE_NUMBER
                    LIST_COPY_CONTACT_STR("Mobile Phone Number", item->contact->mobile_phone);
                    break;
                case 0x3A1D: // PR_RADIO_TELEPHONE_NUMBER
                    LIST_COPY_CONTACT_STR("Radio Phone Number", item->contact->radio_phone);
                    break;
                case 0x3A1E: // PR_CAR_TELEPHONE_NUMBER
                    LIST_COPY_CONTACT_STR("Car Phone Number", item->contact->car_phone);
                    break;
                case 0x3A1F: // PR_OTHER_TELEPHONE_NUMBER
                    LIST_COPY_CONTACT_STR("Other Phone Number", item->contact->other_phone);
                    break;
                case 0x3A20: // PR_TRANSMITTABLE_DISPLAY_NAME
                    LIST_COPY_CONTACT_STR("Transmittable Display Name", item->contact->transmittable_display_name);
                    break;
                case 0x3A21: // PR_PAGER_TELEPHONE_NUMBER
                    LIST_COPY_CONTACT_STR("Pager Phone Number", item->contact->pager_phone);
                    break;
                case 0x3A22: // PR_USER_CERTIFICATE
                    DEBUG_INFO(("User Certificate - NOT PROCESSED\n"));
                    break;
                case 0x3A23: // PR_PRIMARY_FAX_NUMBER
                    LIST_COPY_CONTACT_STR("Primary Fax Number", item->contact->primary_fax);
                    break;
                case 0x3A24: // PR_BUSINESS_FAX_NUMBER
                    LIST_COPY_CONTACT_STR("Business Fax Number", item->contact->business_fax);
                    break;
                case 0x3A25: // PR_HOME_FAX_NUMBER
                    LIST_COPY_CONTACT_STR("Home Fax Number", item->contact->home_fax);
                    break;
                case 0x3A26: // PR_BUSINESS_ADDRESS_COUNTRY
                    LIST_COPY_CONTACT_STR("Business Address Country", item->contact->business_country);
                    break;
                case 0x3A27: // PR_BUSINESS_ADDRESS_CITY
                    LIST_COPY_CONTACT_STR("Business Address City", item->contact->business_city);
                    break;
                case 0x3A28: // PR_BUSINESS_ADDRESS_STATE_OR_PROVINCE
                    LIST_COPY_CONTACT_STR("Business Address State", item->contact->business_state);
                    break;
                case 0x3A29: // PR_BUSINESS_ADDRESS_STREET
                    LIST_COPY_CONTACT_STR("Business Address Street", item->contact->business_street);
                    break;
                case 0x3A2A: // PR_BUSINESS_POSTAL_CODE
                    LIST_COPY_CONTACT_STR("Business Postal Code", item->contact->business_postal_code);
                    break;
                case 0x3A2B: // PR_BUSINESS_PO_BOX
                    LIST_COPY_CONTACT_STR("Business PO Box", item->contact->business_po_box);
                    break;
                case 0x3A2C: // PR_TELEX_NUMBER
                    LIST_COPY_CONTACT_STR("Telex Number", item->contact->telex);
                    break;
                case 0x3A2D: // PR_ISDN_NUMBER
                    LIST_COPY_CONTACT_STR("ISDN Number", item->contact->isdn_phone);
                    break;
                case 0x3A2E: // PR_ASSISTANT_TELEPHONE_NUMBER
                    LIST_COPY_CONTACT_STR("Assistant Phone Number", item->contact->assistant_phone);
                    break;
                case 0x3A2F: // PR_HOME2_TELEPHONE_NUMBER
                    LIST_COPY_CONTACT_STR("Home Phone 2", item->contact->home_phone2);
                    break;
                case 0x3A30: // PR_ASSISTANT
                    LIST_COPY_CONTACT_STR("Assistant's Name", item->contact->assistant_name);
                    break;
                case 0x3A40: // PR_SEND_RICH_INFO
                    LIST_COPY_CONTACT_BOOL("Can receive Rich Text", item->contact->rich_text);
                    break;
                case 0x3A41: // PR_WEDDING_ANNIVERSARY
                    LIST_COPY_CONTACT_TIME("Wedding Anniversary", item->contact->wedding_anniversary);
                    break;
                case 0x3A42: // PR_BIRTHDAY
                    LIST_COPY_CONTACT_TIME("Birthday", item->contact->birthday);
                    break;
                case 0x3A43: // PR_HOBBIES
                    LIST_COPY_CONTACT_STR("Hobbies", item->contact->hobbies);
                    break;
                case 0x3A44: // PR_MIDDLE_NAME
                    LIST_COPY_CONTACT_STR("Middle Name", item->contact->middle_name);
                    break;
                case 0x3A45: // PR_DISPLAY_NAME_PREFIX
                    LIST_COPY_CONTACT_STR("Display Name Prefix (Title)", item->contact->display_name_prefix);
                    break;
                case 0x3A46: // PR_PROFESSION
                    LIST_COPY_CONTACT_STR("Profession", item->contact->profession);
                    break;
                case 0x3A47: // PR_PREFERRED_BY_NAME
                    LIST_COPY_CONTACT_STR("Preferred By Name", item->contact->pref_name);
                    break;
                case 0x3A48: // PR_SPOUSE_NAME
                    LIST_COPY_CONTACT_STR("Spouse's Name", item->contact->spouse_name);
                    break;
                case 0x3A49: // PR_COMPUTER_NETWORK_NAME
                    LIST_COPY_CONTACT_STR("Computer Network Name", item->contact->computer_name);
                    break;
                case 0x3A4A: // PR_CUSTOMER_ID
                    LIST_COPY_CONTACT_STR("Customer ID", item->contact->customer_id);
                    break;
                case 0x3A4B: // PR_TTYTDD_PHONE_NUMBER
                    LIST_COPY_CONTACT_STR("TTY/TDD Phone", item->contact->ttytdd_phone);
                    break;
                case 0x3A4C: // PR_FTP_SITE
                    LIST_COPY_CONTACT_STR("Ftp Site", item->contact->ftp_site);
                    break;
                case 0x3A4D: // PR_GENDER
                    LIST_COPY_CONTACT_ENUM16("Gender", item->contact->gender, 0, 3, "Unspecified", "Female", "Male");
                    break;
                case 0x3A4E: // PR_MANAGER_NAME
                    LIST_COPY_CONTACT_STR("Manager's Name", item->contact->manager_name);
                    break;
                case 0x3A4F: // PR_NICKNAME
                    LIST_COPY_CONTACT_STR("Nickname", item->contact->nickname);
                    break;
                case 0x3A50: // PR_PERSONAL_HOME_PAGE
                    LIST_COPY_CONTACT_STR("Personal Home Page", item->contact->personal_homepage);
                    break;
                case 0x3A51: // PR_BUSINESS_HOME_PAGE
                    LIST_COPY_CONTACT_STR("Business Home Page", item->contact->business_homepage);
                    break;
                case 0x3A57: // PR_COMPANY_MAIN_PHONE_NUMBER
                    LIST_COPY_CONTACT_STR("Company Main Phone", item->contact->company_main_phone);
                    break;
                case 0x3A58: // PR_CHILDRENS_NAMES
                    DEBUG_INFO(("Children's Names - NOT PROCESSED\n"));
                    break;
                case 0x3A59: // PR_HOME_ADDRESS_CITY
                    LIST_COPY_CONTACT_STR("Home Address City", item->contact->home_city);
                    break;
                case 0x3A5A: // PR_HOME_ADDRESS_COUNTRY
                    LIST_COPY_CONTACT_STR("Home Address Country", item->contact->home_country);
                    break;
                case 0x3A5B: // PR_HOME_ADDRESS_POSTAL_CODE
                    LIST_COPY_CONTACT_STR("Home Address Postal Code", item->contact->home_postal_code);
                    break;
                case 0x3A5C: // PR_HOME_ADDRESS_STATE_OR_PROVINCE
                    LIST_COPY_CONTACT_STR("Home Address State or Province", item->contact->home_state);
                    break;
                case 0x3A5D: // PR_HOME_ADDRESS_STREET
                    LIST_COPY_CONTACT_STR("Home Address Street", item->contact->home_street);
                    break;
                case 0x3A5E: // PR_HOME_ADDRESS_POST_OFFICE_BOX
                    LIST_COPY_CONTACT_STR("Home Address Post Office Box", item->contact->home_po_box);
                    break;
                case 0x3A5F: // PR_OTHER_ADDRESS_CITY
                    LIST_COPY_CONTACT_STR("Other Address City", item->contact->other_city);
                    break;
                case 0x3A60: // PR_OTHER_ADDRESS_COUNTRY
                    LIST_COPY_CONTACT_STR("Other Address Country", item->contact->other_country);
                    break;
                case 0x3A61: // PR_OTHER_ADDRESS_POSTAL_CODE
                    LIST_COPY_CONTACT_STR("Other Address Postal Code", item->contact->other_postal_code);
                    break;
                case 0x3A62: // PR_OTHER_ADDRESS_STATE_OR_PROVINCE
                    LIST_COPY_CONTACT_STR("Other Address State", item->contact->other_state);
                    break;
                case 0x3A63: // PR_OTHER_ADDRESS_STREET
                    LIST_COPY_CONTACT_STR("Other Address Street", item->contact->other_street);
                    break;
                case 0x3A64: // PR_OTHER_ADDRESS_POST_OFFICE_BOX
                    LIST_COPY_CONTACT_STR("Other Address Post Office box", item->contact->other_po_box);
                    break;
                case 0x3FDE: // PR_INTERNET_CPID
                    LIST_COPY_INT32("Internet code page", item->internet_cpid);
                    break;
                case 0x3FFD: // PR_MESSAGE_CODEPAGE
                    LIST_COPY_INT32("Message code page", item->message_codepage);
                    break;
                case 0x65E3: // PR_PREDECESSOR_CHANGE_LIST
                    LIST_COPY_BIN(item->predecessor_change);
                    DEBUG_INFO(("Predecessor Change\n"));
                    DEBUG_HEXDUMP(item->predecessor_change.data, item->predecessor_change.size);
                    break;
                case 0x67F2: // ID2 value of the attachment
                    NULL_CHECK(attach);
                    LIST_COPY_INT32("Attachment ID2 value", ut);
                    attach->id2_val = ut;
                    break;
                case 0x67FF: // Extra Property Identifier (Password CheckSum)
                    LIST_COPY_STORE_INT32("Password checksum", item->message_store->pwd_chksum);
                    break;
                case 0x6F02: // Secure HTML Body
                    LIST_COPY_EMAIL_BIN("Secure HTML Body", item->email->encrypted_htmlbody);
                    break;
                case 0x6F04: // Secure Text Body
                    LIST_COPY_EMAIL_BIN("Secure Text Body", item->email->encrypted_body);
                    break;
                case 0x7C07: // top of folders ENTRYID
                    LIST_COPY_STORE_ENTRYID("Top of folders RecID", item->message_store->top_of_folder);
                    break;
                case 0x8005: // Contact's Fullname
                    LIST_COPY_CONTACT_STR("Contact Fullname", item->contact->fullname);
                    break;
                case 0x801A: // Full Home Address
                    LIST_COPY_CONTACT_STR("Home Address", item->contact->home_address);
                    break;
                case 0x801B: // Full Business Address
                    LIST_COPY_CONTACT_STR("Business Address", item->contact->business_address);
                    break;
                case 0x801C: // Full Other Address
                    LIST_COPY_CONTACT_STR("Other Address", item->contact->other_address);
                    break;
                case 0x8045: // Work address street
                    LIST_COPY_CONTACT_STR("Work address street", item->contact->work_address_street);
                    break;
                case 0x8046: // Work address city
                    LIST_COPY_CONTACT_STR("Work address city", item->contact->work_address_city);
                    break;
                case 0x8047: // Work address state
                    LIST_COPY_CONTACT_STR("Work address state", item->contact->work_address_state);
                    break;
                case 0x8048: // Work address postalcode
                    LIST_COPY_CONTACT_STR("Work address postalcode", item->contact->work_address_postalcode);
                    break;
                case 0x8049: // Work address country
                    LIST_COPY_CONTACT_STR("Work address country", item->contact->work_address_country);
                    break;
                case 0x804A: // Work address postofficebox
                    LIST_COPY_CONTACT_STR("Work address postofficebox", item->contact->work_address_postofficebox);
                    break;
                case 0x8082: // Email Address 1 Transport
                    LIST_COPY_CONTACT_STR("Email Address 1 Transport", item->contact->address1_transport);
                    break;
                case 0x8083: // Email Address 1 Address
                    LIST_COPY_CONTACT_STR("Email Address 1 Address", item->contact->address1);
                    break;
                case 0x8084: // Email Address 1 Description
                    LIST_COPY_CONTACT_STR("Email Address 1 Description", item->contact->address1_desc);
                    break;
                case 0x8085: // Email Address 1 Record
                    LIST_COPY_CONTACT_STR("Email Address 1 Record", item->contact->address1a);
                    break;
                case 0x8092: // Email Address 2 Transport
                    LIST_COPY_CONTACT_STR("Email Address 2 Transport", item->contact->address2_transport);
                    break;
                case 0x8093: // Email Address 2 Address
                    LIST_COPY_CONTACT_STR("Email Address 2 Address", item->contact->address2);
                    break;
                case 0x8094: // Email Address 2 Description
                    LIST_COPY_CONTACT_STR("Email Address 2 Description", item->contact->address2_desc);
                    break;
                case 0x8095: // Email Address 2 Record
                    LIST_COPY_CONTACT_STR("Email Address 2 Record", item->contact->address2a);
                    break;
                case 0x80A2: // Email Address 3 Transport
                    LIST_COPY_CONTACT_STR("Email Address 3 Transport", item->contact->address3_transport);
                    break;
                case 0x80A3: // Email Address 3 Address
                    LIST_COPY_CONTACT_STR("Email Address 3 Address", item->contact->address3);
                    break;
                case 0x80A4: // Email Address 3 Description
                    LIST_COPY_CONTACT_STR("Email Address 3 Description", item->contact->address3_desc);
                    break;
                case 0x80A5: // Email Address 3 Record
                    LIST_COPY_CONTACT_STR("Email Address 3 Record", item->contact->address3a);
                    break;
                case 0x80D8: // Internet Free/Busy
                    LIST_COPY_CONTACT_STR("Internet Free/Busy", item->contact->free_busy_address);
                    break;
                case 0x8205: // PR_OUTLOOK_EVENT_SHOW_TIME_AS
                    LIST_COPY_APPT_ENUM("Appointment shows as", item->appointment->showas, 0, 4,
                        "Free", "Tentative", "Busy", "Out Of Office");
                    break;
                case 0x8208: // PR_OUTLOOK_EVENT_LOCATION
                    LIST_COPY_APPT_STR("Appointment Location", item->appointment->location);
                    break;
                case 0x820d: // PR_OUTLOOK_EVENT_START_DATE
                    LIST_COPY_APPT_TIME("Appointment Date Start", item->appointment->start);
                    break;
                case 0x820e: // PR_OUTLOOK_EVENT_START_END
                    LIST_COPY_APPT_TIME("Appointment Date End", item->appointment->end);
                    break;
                case 0x8214: // Label for an appointment
                    LIST_COPY_APPT_ENUM("Label for appointment", item->appointment->label, 0, 11,
                        "None",
                        "Important",
                        "Business",
                        "Personal",
                        "Vacation",
                        "Must Attend",
                        "Travel Required",
                        "Needs Preparation",
                        "Birthday",
                        "Anniversary",
                        "Phone Call");
                    break;
                case 0x8215: // PR_OUTLOOK_EVENT_ALL_DAY
                    LIST_COPY_APPT_BOOL("All day flag", item->appointment->all_day);
                    break;
                case 0x8216: // PR_OUTLOOK_EVENT_RECURRENCE_DATA
                    LIST_COPY_APPT_BIN("Appointment recurrence data", item->appointment->recurrence_data);
                    break;
                case 0x8223: // PR_OUTLOOK_EVENT_IS_RECURRING
                    LIST_COPY_APPT_BOOL("Is recurring", item->appointment->is_recurring);
                    break;
                case 0x8231: // Recurrence type
                    LIST_COPY_APPT_ENUM("Appointment recurrence type ", item->appointment->recurrence_type, 0, 5,
                        "None",
                        "Daily",
                        "Weekly",
                        "Monthly",
                        "Yearly");
                    break;
                case 0x8232: // Recurrence description
                    LIST_COPY_APPT_STR("Appointment recurrence description", item->appointment->recurrence_description);
                    break;
                case 0x8234: // TimeZone as String
                    LIST_COPY_APPT_STR("TimeZone of times", item->appointment->timezonestring);
                    break;
                case 0x8235: // PR_OUTLOOK_EVENT_RECURRENCE_START
                    LIST_COPY_APPT_TIME("Recurrence Start Date", item->appointment->recurrence_start);
                    break;
                case 0x8236: // PR_OUTLOOK_EVENT_RECURRENCE_END
                    LIST_COPY_APPT_TIME("Recurrence End Date", item->appointment->recurrence_end);
                    break;
                case 0x8501: // PR_OUTLOOK_COMMON_REMINDER_MINUTES_BEFORE
                    LIST_COPY_APPT_INT32("Alarm minutes", item->appointment->alarm_minutes);
                    break;
                case 0x8503: // PR_OUTLOOK_COMMON_REMINDER_SET
                    LIST_COPY_APPT_BOOL("Reminder alarm", item->appointment->alarm);
                    break;
                case 0x8516: // Common start
                    DEBUG_INFO(("Common Start Date - %s\n", pst_fileTimeToAscii((FILETIME*)list->elements[x]->data, time_buffer)));
                    break;
                case 0x8517: // Common end
                    DEBUG_INFO(("Common End Date - %s\n", pst_fileTimeToAscii((FILETIME*)list->elements[x]->data, time_buffer)));
                    break;
                case 0x851f: // Play reminder sound filename
                    LIST_COPY_APPT_STR("Appointment reminder sound filename", item->appointment->alarm_filename);
                    break;
                case 0x8530: // Followup
                    LIST_COPY_CONTACT_STR("Followup String", item->contact->followup);
                    break;
                case 0x8534: // Mileage
                    LIST_COPY_CONTACT_STR("Mileage", item->contact->mileage);
                    break;
                case 0x8535: // Billing Information
                    LIST_COPY_CONTACT_STR("Billing Information", item->contact->billing_information);
                    break;
                case 0x8554: // PR_OUTLOOK_VERSION
                    LIST_COPY_STR("Outlook Version", item->outlook_version);
                    break;
                case 0x8560: // Appointment Reminder Time
                    LIST_COPY_APPT_TIME("Appointment Reminder Time", item->appointment->reminder);
                    break;
                case 0x8700: // Journal Type
                    LIST_COPY_JOURNAL_STR("Journal Entry Type", item->journal->type);
                    break;
                case 0x8706: // Journal Start date/time
                    LIST_COPY_JOURNAL_TIME("Start Timestamp", item->journal->start);
                    break;
                case 0x8708: // Journal End date/time
                    LIST_COPY_JOURNAL_TIME("End Timestamp", item->journal->end);
                    break;
                case 0x8712: // Journal Type Description
                    LIST_COPY_JOURNAL_STR("Journal description", item->journal->description);
                    break;
                default:
                    if (list->elements[x]->type == (uint32_t)0x0002) {
                        DEBUG_WARN(("Unknown type %#x 16bit int = %hi\n", list->elements[x]->mapi_id,
                            *(int16_t*)list->elements[x]->data));

                    } else if (list->elements[x]->type == (uint32_t)0x0003) {
                        DEBUG_WARN(("Unknown type %#x 32bit int = %i\n", list->elements[x]->mapi_id,
                            *(int32_t*)list->elements[x]->data));

                    } else if (list->elements[x]->type == (uint32_t)0x0004) {
                        DEBUG_WARN(("Unknown type %#x 4-byte floating [size = %#x]\n", list->elements[x]->mapi_id,
                            list->elements[x]->size));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);

                    } else if (list->elements[x]->type == (uint32_t)0x0005) {
                        DEBUG_WARN(("Unknown type %#x double floating [size = %#x]\n", list->elements[x]->mapi_id,
                            list->elements[x]->size));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);

                    } else if (list->elements[x]->type == (uint32_t)0x0006) {
                        DEBUG_WARN(("Unknown type %#x signed 64bit int = %"PRIi64"\n", list->elements[x]->mapi_id,
                            *(int64_t*)list->elements[x]->data));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);

                    } else if (list->elements[x]->type == (uint32_t)0x0007) {
                        DEBUG_WARN(("Unknown type %#x application time [size = %#x]\n", list->elements[x]->mapi_id,
                            list->elements[x]->size));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);

                    } else if (list->elements[x]->type == (uint32_t)0x000a) {
                        DEBUG_WARN(("Unknown type %#x 32bit error value = %i\n", list->elements[x]->mapi_id,
                            *(int32_t*)list->elements[x]->data));

                    } else if (list->elements[x]->type == (uint32_t)0x000b) {
                        DEBUG_WARN(("Unknown type %#x 16bit boolean = %s [%hi]\n", list->elements[x]->mapi_id,
                            (*((int16_t*)list->elements[x]->data)!=0?"True":"False"),
                            *((int16_t*)list->elements[x]->data)));

                    } else if (list->elements[x]->type == (uint32_t)0x000d) {
                        DEBUG_WARN(("Unknown type %#x Embedded object [size = %#x]\n", list->elements[x]->mapi_id,
                            list->elements[x]->size));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);

                    } else if (list->elements[x]->type == (uint32_t)0x0014) {
                        DEBUG_WARN(("Unknown type %#x signed 64bit int = %"PRIi64"\n", list->elements[x]->mapi_id,
                            *(int64_t*)list->elements[x]->data));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);

                    } else if (list->elements[x]->type == (uint32_t)0x001e) {
                        DEBUG_WARN(("Unknown type %#x String Data = \"%s\"\n", list->elements[x]->mapi_id,
                            list->elements[x]->data));

                    } else if (list->elements[x]->type == (uint32_t)0x001f) {
                        DEBUG_WARN(("Unknown type %#x Unicode String Data [size = %#x]\n", list->elements[x]->mapi_id,
                            list->elements[x]->size));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);

                    } else if (list->elements[x]->type == (uint32_t)0x0040) {
                        DEBUG_WARN(("Unknown type %#x Date = \"%s\"\n", list->elements[x]->mapi_id,
                            pst_fileTimeToAscii((FILETIME*)list->elements[x]->data, time_buffer)));

                    } else if (list->elements[x]->type == (uint32_t)0x0048) {
                        DEBUG_WARN(("Unknown type %#x OLE GUID [size = %#x]\n", list->elements[x]->mapi_id,
                            list->elements[x]->size));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);

                    } else if (list->elements[x]->type == (uint32_t)0x0102) {
                        DEBUG_WARN(("Unknown type %#x Binary Data [size = %#x]\n", list->elements[x]->mapi_id,
                            list->elements[x]->size));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);

                    } else if (list->elements[x]->type == (uint32_t)0x1003) {
                        DEBUG_WARN(("Unknown type %#x Array of 32 bit values [size = %#x]\n", list->elements[x]->mapi_id,
                            list->elements[x]->size));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);

                    } else if (list->elements[x]->type == (uint32_t)0x1014) {
                        DEBUG_WARN(("Unknown type %#x Array of 64 bit values [siize = %#x]\n", list->elements[x]->mapi_id,
                            list->elements[x]->size));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);

                    } else if (list->elements[x]->type == (uint32_t)0x101e) {
                        DEBUG_WARN(("Unknown type %#x Array of Strings [size = %#x]\n", list->elements[x]->mapi_id,
                            list->elements[x]->size));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);

                    } else if (list->elements[x]->type == (uint32_t)0x101f) {
                        DEBUG_WARN(("Unknown type %#x Array of Unicode Strings [size = %#x]\n", list->elements[x]->mapi_id,
                            list->elements[x]->size));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);

                    } else if (list->elements[x]->type == (uint32_t)0x1102) {
                        DEBUG_WARN(("Unknown type %#x Array of binary data blobs [size = %#x]\n", list->elements[x]->mapi_id,
                            list->elements[x]->size));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);

                    } else {
                        DEBUG_WARN(("Unknown type %#x Not Printable [%#x]\n", list->elements[x]->mapi_id,
                            list->elements[x]->type));
                        DEBUG_HEXDUMP(list->elements[x]->data, list->elements[x]->size);
                    }

                    if (list->elements[x]->data) {
                        free(list->elements[x]->data);
                        list->elements[x]->data = NULL;
                    }
            }
        }
        list = list->next;
        if (attach) attach = attach->next;
    }
    DEBUG_RET();
    return 0;
}


static void pst_free_list(pst_mapi_object *list) {
    pst_mapi_object *l;
    DEBUG_ENT("pst_free_list");
    while (list) {
        if (list->elements) {
            int32_t x;
            for (x=0; x < list->orig_count; x++) {
                if (list->elements[x]) {
                    if (list->elements[x]->data) free(list->elements[x]->data);
                    free(list->elements[x]);
                }
            }
            free(list->elements);
        }
        l = list->next;
        free (list);
        list = l;
    }
    DEBUG_RET();
}


static void pst_free_id2(pst_id2_tree * head) {
    pst_id2_tree *t;
    DEBUG_ENT("pst_free_id2");
    while (head) {
        pst_free_id2(head->child);
        t = head->next;
        free(head);
        head = t;
    }
    DEBUG_RET();
}


static void pst_free_id (pst_index_ll *head) {
    pst_index_ll *t;
    DEBUG_ENT("pst_free_id");
    while (head) {
        t = head->next;
        free(head);
        head = t;
    }
    DEBUG_RET();
}


static void pst_free_desc (pst_desc_tree *head) {
    pst_desc_tree *t;
    DEBUG_ENT("pst_free_desc");
    while (head) {
        pst_free_desc(head->child);
        t = head->next;
        free(head);
        head = t;
    }
    DEBUG_RET();
}


static void pst_free_xattrib(pst_x_attrib_ll *x) {
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


static pst_id2_tree * pst_build_id2(pst_file *pf, pst_index_ll* list) {
    pst_block_header block_head;
    pst_id2_tree *head = NULL, *tail = NULL;
    uint16_t x = 0;
    char *b_ptr = NULL;
    char *buf = NULL;
    pst_id2_assoc id2_rec;
    pst_index_ll *i_ptr = NULL;
    pst_id2_tree *i2_ptr = NULL;
    DEBUG_ENT("pst_build_id2");

    if (pst_read_block_size(pf, list->offset, list->size, &buf) < list->size) {
        //an error occured in block read
        DEBUG_WARN(("block read error occured. offset = %#"PRIx64", size = %#"PRIx64"\n", list->offset, list->size));
        if (buf) free(buf);
        DEBUG_RET();
        return NULL;
    }
    DEBUG_HEXDUMPC(buf, list->size, 16);

    memcpy(&block_head, buf, sizeof(block_head));
    LE16_CPU(block_head.type);
    LE16_CPU(block_head.count);

    if (block_head.type != (uint16_t)0x0002) { // some sort of constant?
        DEBUG_WARN(("Unknown constant [%#hx] at start of id2 values [offset %#"PRIx64"].\n", block_head.type, list->offset));
        if (buf) free(buf);
        DEBUG_RET();
        return NULL;
    }

    DEBUG_INFO(("ID %#"PRIx64" is likely to be a description record. Count is %i (offset %#"PRIx64")\n",
            list->i_id, block_head.count, list->offset));
    x = 0;
    b_ptr = buf + ((pf->do_read64) ? 0x08 : 0x04);
    while (x < block_head.count) {
        b_ptr += pst_decode_assoc(pf, &id2_rec, b_ptr);
        DEBUG_INFO(("id2 = %#x, id = %#"PRIx64", child id = %#"PRIx64"\n", id2_rec.id2, id2_rec.id, id2_rec.child_id));
        if ((i_ptr = pst_getID(pf, id2_rec.id)) == NULL) {
            DEBUG_WARN(("%#"PRIx64" - Not Found\n", id2_rec.id));
        } else {
            DEBUG_INFO(("%#"PRIx64" - Offset %#"PRIx64", u1 %#"PRIx64", Size %"PRIi64"(%#"PRIx64")\n",
                         i_ptr->i_id, i_ptr->offset, i_ptr->u1, i_ptr->size, i_ptr->size));
            // add it to the tree
            i2_ptr = (pst_id2_tree*) pst_malloc(sizeof(pst_id2_tree));
            i2_ptr->id2   = id2_rec.id2;
            i2_ptr->id    = i_ptr;
            i2_ptr->child = NULL;
            i2_ptr->next  = NULL;
            if (!head) head = i2_ptr;
            if (tail)  tail->next = i2_ptr;
            tail = i2_ptr;
            if (id2_rec.child_id) {
                if ((i_ptr = pst_getID(pf, id2_rec.child_id)) == NULL) {
                    DEBUG_WARN(("child id [%#"PRIx64"] not found\n", id2_rec.child_id));
                }
                else {
                    i2_ptr->child = pst_build_id2(pf, i_ptr);
                }
            }
        }
        x++;
    }
    if (buf) free (buf);
    DEBUG_RET();
    return head;
}


static void pst_free_attach(pst_item_attach *attach) {
    while (attach) {
        pst_item_attach *t;
        SAFE_FREE_STR(attach->filename1);
        SAFE_FREE_STR(attach->filename2);
        SAFE_FREE_STR(attach->mimetype);
        SAFE_FREE_BIN(attach->data);
        pst_free_id2(attach->id2_head);
        t = attach->next;
        free(attach);
        attach = t;
    }
}


void pst_freeItem(pst_item *item) {
    pst_item_extra_field *et;

    DEBUG_ENT("pst_freeItem");
    if (item) {
        if (item->email) {
            SAFE_FREE(item->email->arrival_date);
            SAFE_FREE_STR(item->email->cc_address);
            SAFE_FREE_STR(item->email->bcc_address);
            SAFE_FREE_BIN(item->email->conversation_index);
            SAFE_FREE_BIN(item->email->encrypted_body);
            SAFE_FREE_BIN(item->email->encrypted_htmlbody);
            SAFE_FREE_STR(item->email->header);
            SAFE_FREE_STR(item->email->htmlbody);
            SAFE_FREE_STR(item->email->in_reply_to);
            SAFE_FREE_STR(item->email->messageid);
            SAFE_FREE_STR(item->email->original_bcc);
            SAFE_FREE_STR(item->email->original_cc);
            SAFE_FREE_STR(item->email->original_to);
            SAFE_FREE_STR(item->email->outlook_recipient);
            SAFE_FREE_STR(item->email->outlook_recipient_name);
            SAFE_FREE_STR(item->email->outlook_recipient2);
            SAFE_FREE_STR(item->email->outlook_sender);
            SAFE_FREE_STR(item->email->outlook_sender_name);
            SAFE_FREE_STR(item->email->outlook_sender2);
            SAFE_FREE_STR(item->email->processed_subject);
            SAFE_FREE_STR(item->email->recip_access);
            SAFE_FREE_STR(item->email->recip_address);
            SAFE_FREE_STR(item->email->recip2_access);
            SAFE_FREE_STR(item->email->recip2_address);
            SAFE_FREE_STR(item->email->reply_to);
            SAFE_FREE_STR(item->email->rtf_body_tag);
            SAFE_FREE_BIN(item->email->rtf_compressed);
            SAFE_FREE_STR(item->email->return_path_address);
            SAFE_FREE_STR(item->email->sender_access);
            SAFE_FREE_STR(item->email->sender_address);
            SAFE_FREE_STR(item->email->sender2_access);
            SAFE_FREE_STR(item->email->sender2_address);
            SAFE_FREE(item->email->sent_date);
            SAFE_FREE(item->email->sentmail_folder);
            SAFE_FREE_STR(item->email->sentto_address);
            SAFE_FREE_STR(item->email->report_text);
            SAFE_FREE(item->email->report_time);
            SAFE_FREE_STR(item->email->supplementary_info);
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
            SAFE_FREE_STR(item->contact->account_name);
            SAFE_FREE_STR(item->contact->address1);
            SAFE_FREE_STR(item->contact->address1a);
            SAFE_FREE_STR(item->contact->address1_desc);
            SAFE_FREE_STR(item->contact->address1_transport);
            SAFE_FREE_STR(item->contact->address2);
            SAFE_FREE_STR(item->contact->address2a);
            SAFE_FREE_STR(item->contact->address2_desc);
            SAFE_FREE_STR(item->contact->address2_transport);
            SAFE_FREE_STR(item->contact->address3);
            SAFE_FREE_STR(item->contact->address3a);
            SAFE_FREE_STR(item->contact->address3_desc);
            SAFE_FREE_STR(item->contact->address3_transport);
            SAFE_FREE_STR(item->contact->assistant_name);
            SAFE_FREE_STR(item->contact->assistant_phone);
            SAFE_FREE_STR(item->contact->billing_information);
            SAFE_FREE(item->contact->birthday);
            SAFE_FREE_STR(item->contact->business_address);
            SAFE_FREE_STR(item->contact->business_city);
            SAFE_FREE_STR(item->contact->business_country);
            SAFE_FREE_STR(item->contact->business_fax);
            SAFE_FREE_STR(item->contact->business_homepage);
            SAFE_FREE_STR(item->contact->business_phone);
            SAFE_FREE_STR(item->contact->business_phone2);
            SAFE_FREE_STR(item->contact->business_po_box);
            SAFE_FREE_STR(item->contact->business_postal_code);
            SAFE_FREE_STR(item->contact->business_state);
            SAFE_FREE_STR(item->contact->business_street);
            SAFE_FREE_STR(item->contact->callback_phone);
            SAFE_FREE_STR(item->contact->car_phone);
            SAFE_FREE_STR(item->contact->company_main_phone);
            SAFE_FREE_STR(item->contact->company_name);
            SAFE_FREE_STR(item->contact->computer_name);
            SAFE_FREE_STR(item->contact->customer_id);
            SAFE_FREE_STR(item->contact->def_postal_address);
            SAFE_FREE_STR(item->contact->department);
            SAFE_FREE_STR(item->contact->display_name_prefix);
            SAFE_FREE_STR(item->contact->first_name);
            SAFE_FREE_STR(item->contact->followup);
            SAFE_FREE_STR(item->contact->free_busy_address);
            SAFE_FREE_STR(item->contact->ftp_site);
            SAFE_FREE_STR(item->contact->fullname);
            SAFE_FREE_STR(item->contact->gov_id);
            SAFE_FREE_STR(item->contact->hobbies);
            SAFE_FREE_STR(item->contact->home_address);
            SAFE_FREE_STR(item->contact->home_city);
            SAFE_FREE_STR(item->contact->home_country);
            SAFE_FREE_STR(item->contact->home_fax);
            SAFE_FREE_STR(item->contact->home_po_box);
            SAFE_FREE_STR(item->contact->home_phone);
            SAFE_FREE_STR(item->contact->home_phone2);
            SAFE_FREE_STR(item->contact->home_postal_code);
            SAFE_FREE_STR(item->contact->home_state);
            SAFE_FREE_STR(item->contact->home_street);
            SAFE_FREE_STR(item->contact->initials);
            SAFE_FREE_STR(item->contact->isdn_phone);
            SAFE_FREE_STR(item->contact->job_title);
            SAFE_FREE_STR(item->contact->keyword);
            SAFE_FREE_STR(item->contact->language);
            SAFE_FREE_STR(item->contact->location);
            SAFE_FREE_STR(item->contact->manager_name);
            SAFE_FREE_STR(item->contact->middle_name);
            SAFE_FREE_STR(item->contact->mileage);
            SAFE_FREE_STR(item->contact->mobile_phone);
            SAFE_FREE_STR(item->contact->nickname);
            SAFE_FREE_STR(item->contact->office_loc);
            SAFE_FREE_STR(item->contact->common_name);
            SAFE_FREE_STR(item->contact->org_id);
            SAFE_FREE_STR(item->contact->other_address);
            SAFE_FREE_STR(item->contact->other_city);
            SAFE_FREE_STR(item->contact->other_country);
            SAFE_FREE_STR(item->contact->other_phone);
            SAFE_FREE_STR(item->contact->other_po_box);
            SAFE_FREE_STR(item->contact->other_postal_code);
            SAFE_FREE_STR(item->contact->other_state);
            SAFE_FREE_STR(item->contact->other_street);
            SAFE_FREE_STR(item->contact->pager_phone);
            SAFE_FREE_STR(item->contact->personal_homepage);
            SAFE_FREE_STR(item->contact->pref_name);
            SAFE_FREE_STR(item->contact->primary_fax);
            SAFE_FREE_STR(item->contact->primary_phone);
            SAFE_FREE_STR(item->contact->profession);
            SAFE_FREE_STR(item->contact->radio_phone);
            SAFE_FREE_STR(item->contact->spouse_name);
            SAFE_FREE_STR(item->contact->suffix);
            SAFE_FREE_STR(item->contact->surname);
            SAFE_FREE_STR(item->contact->telex);
            SAFE_FREE_STR(item->contact->transmittable_display_name);
            SAFE_FREE_STR(item->contact->ttytdd_phone);
            SAFE_FREE(item->contact->wedding_anniversary);
            SAFE_FREE_STR(item->contact->work_address_street);
            SAFE_FREE_STR(item->contact->work_address_city);
            SAFE_FREE_STR(item->contact->work_address_state);
            SAFE_FREE_STR(item->contact->work_address_postalcode);
            SAFE_FREE_STR(item->contact->work_address_country);
            SAFE_FREE_STR(item->contact->work_address_postofficebox);
            free(item->contact);
        }

        pst_free_attach(item->attach);

        while (item->extra_fields) {
            SAFE_FREE(item->extra_fields->field_name);
            SAFE_FREE(item->extra_fields->value);
            et = item->extra_fields->next;
            free(item->extra_fields);
            item->extra_fields = et;
        }
        if (item->journal) {
            SAFE_FREE(item->journal->start);
            SAFE_FREE(item->journal->end);
            SAFE_FREE_STR(item->journal->type);
            free(item->journal);
        }
        if (item->appointment) {
            SAFE_FREE(item->appointment->start);
            SAFE_FREE(item->appointment->end);
            SAFE_FREE_STR(item->appointment->location);
            SAFE_FREE(item->appointment->reminder);
            SAFE_FREE_STR(item->appointment->alarm_filename);
            SAFE_FREE_STR(item->appointment->timezonestring);
            SAFE_FREE_STR(item->appointment->recurrence_description);
            SAFE_FREE_BIN(item->appointment->recurrence_data);
            SAFE_FREE(item->appointment->recurrence_start);
            SAFE_FREE(item->appointment->recurrence_end);
            free(item->appointment);
        }
        SAFE_FREE(item->ascii_type);
        SAFE_FREE_STR(item->body_charset);
        SAFE_FREE_STR(item->body);
        SAFE_FREE_STR(item->subject);
        SAFE_FREE_STR(item->comment);
        SAFE_FREE(item->create_date);
        SAFE_FREE_STR(item->file_as);
        SAFE_FREE(item->modify_date);
        SAFE_FREE_STR(item->outlook_version);
        SAFE_FREE_BIN(item->record_key);
        SAFE_FREE_BIN(item->predecessor_change);
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
static int pst_getBlockOffsetPointer(pst_file *pf, pst_id2_tree *i2_head, pst_subblocks *subblocks, uint32_t offset, pst_block_offset_pointer *p) {
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


/** */
static int pst_getBlockOffset(char *buf, size_t read_size, uint32_t i_offset, uint32_t offset, pst_block_offset *p) {
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
        DEBUG_WARN(("get block offset from > to\n"));
        DEBUG_RET();
        return 0;
    }
    DEBUG_RET();
    return 1;
}


/** */
pst_index_ll* pst_getID(pst_file* pf, uint64_t i_id) {
    pst_index_ll *ptr;
    DEBUG_ENT("pst_getID");
    if (i_id == 0) {
        DEBUG_RET();
        return NULL;
    }

    //if (i_id & 1) DEBUG_INFO(("have odd id bit %#"PRIx64"\n", i_id));
    //if (i_id & 2) DEBUG_INFO(("have two id bit %#"PRIx64"\n", i_id));
    i_id -= (i_id & 1);

    DEBUG_INFO(("Trying to find %#"PRIx64"\n", i_id));
    ptr = pf->i_head;
    while (ptr && (ptr->i_id != i_id)) {
        ptr = ptr->next;
    }
    if (ptr) {DEBUG_INFO(("Found Value %#"PRIx64"\n", i_id));            }
    else     {DEBUG_INFO(("ERROR: Value %#"PRIx64" not found\n", i_id)); }
    DEBUG_RET();
    return ptr;
}


static pst_id2_tree *pst_getID2(pst_id2_tree *head, uint64_t id2) {
    DEBUG_ENT("pst_getID2");
    DEBUG_INFO(("looking for id2 = %#"PRIx64"\n", id2));
    pst_id2_tree *ptr = head;
    while (ptr) {
        if (ptr->id2 == id2) break;
        if (ptr->child) {
            pst_id2_tree *rc = pst_getID2(ptr->child, id2);
            if (rc) {
                DEBUG_RET();
                return rc;
            }
        }
        ptr = ptr->next;
    }
    if (ptr && ptr->id) {
        DEBUG_INFO(("Found value %#"PRIx64"\n", ptr->id->i_id));
        DEBUG_RET();
        return ptr;
    }
    DEBUG_INFO(("ERROR Not Found\n"));
    DEBUG_RET();
    return NULL;
}


/**
 * find the id in the descriptor tree rooted at pf->d_head
 *
 * @param pf    global pst file pointer
 * @param d_id  the id we are looking for
 *
 * @return pointer to the pst_desc_tree node in the descriptor tree
*/
static pst_desc_tree* pst_getDptr(pst_file *pf, uint64_t d_id) {
    pst_desc_tree *ptr = pf->d_head;
    DEBUG_ENT("pst_getDptr");
    while (ptr && (ptr->d_id != d_id)) {
        //DEBUG_INFO(("Looking for %#"PRIx64" at node %#"PRIx64" with parent %#"PRIx64"\n", id, ptr->d_id, ptr->parent_d_id));
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


static void pst_printDptr(pst_file *pf, pst_desc_tree *ptr) {
    DEBUG_ENT("pst_printDptr");
    while (ptr) {
        DEBUG_INFO(("%#"PRIx64" [%i] desc=%#"PRIx64", assoc tree=%#"PRIx64"\n", ptr->d_id, ptr->no_child,
                    (ptr->desc       ? ptr->desc->i_id       : (uint64_t)0),
                    (ptr->assoc_tree ? ptr->assoc_tree->i_id : (uint64_t)0)));
        if (ptr->child) {
            pst_printDptr(pf, ptr->child);
        }
        ptr = ptr->next;
    }
    DEBUG_RET();
}


static void pst_printID2ptr(pst_id2_tree *ptr) {
    DEBUG_ENT("pst_printID2ptr");
    while (ptr) {
        DEBUG_INFO(("%#"PRIx64" id=%#"PRIx64"\n", ptr->id2, (ptr->id ? ptr->id->i_id : (uint64_t)0)));
        if (ptr->child) pst_printID2ptr(ptr->child);
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
static size_t pst_read_block_size(pst_file *pf, int64_t offset, size_t size, char **buf) {
    size_t rsize;
    DEBUG_ENT("pst_read_block_size");
    DEBUG_INFO(("Reading block from %#"PRIx64", %x bytes\n", offset, size));

    if (*buf) {
        DEBUG_INFO(("Freeing old memory\n"));
        free(*buf);
    }
    *buf = (char*) pst_malloc(size);

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


/** Decrypt a block of data from the pst file.
 * @param i_id identifier of this block, needed as part of the key for the enigma cipher
 * @param buf  pointer to the buffer to be decrypted in place
 * @param size size of the buffer
 * @param type
    @li 0 PST_NO_ENCRYPT, none
    @li 1 PST_COMP_ENCRYPT, simple byte substitution cipher with fixed key
    @li 2 PST_ENCRYPT, german enigma 3 rotor cipher with fixed key
 * @return 0 if ok, -1 if error (NULL buffer or unknown encryption type)
 */
static int pst_decrypt(uint64_t i_id, char *buf, size_t size, unsigned char type) {
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
        uint16_t salt = (uint16_t) (((i_id & 0x00000000ffff0000) >> 16) ^ (i_id & 0x000000000000ffff));
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
        DEBUG_WARN(("Unknown encryption: %i. Cannot decrypt\n", type));
        DEBUG_RET();
        return -1;
    }
    DEBUG_RET();
    return 0;
}


static uint64_t pst_getIntAt(pst_file *pf, char *buf) {
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


static uint64_t pst_getIntAtPos(pst_file *pf, int64_t pos ) {
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
static size_t pst_getAtPos(pst_file *pf, int64_t pos, void* buf, size_t size) {
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
//      p = (pst_block_recorder*)pst_malloc(sizeof(*p));
//      *t = p;
//      p->next      = tail;
//      p->offset    = pos;
//      p->size      = size;
//      p->readcount = 1;
//  }
//  DEBUG_INFO(("pst file old offset %#"PRIx64" old size %#x read count %i offset %#"PRIx64" size %#x\n",
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
 * Get an ID block from file using pst_ff_getIDblock() and decrypt if necessary
 * @param pf   PST file structure
 * @param i_id ID of block to retrieve
 * @param buf  reference to pointer to buffer that will contain the data block.
 *             If this pointer is non-NULL, it will first be free()d.
 * @return     Size of block read into memory
 */
size_t pst_ff_getIDblock_dec(pst_file *pf, uint64_t i_id, char **buf) {
    size_t r;
    int noenc = (int)(i_id & 2);   // disable encryption
    DEBUG_ENT("pst_ff_getIDblock_dec");
    DEBUG_INFO(("for id %#"PRIx64"\n", i_id));
    r = pst_ff_getIDblock(pf, i_id, buf);
    if ((pf->encryption) && !(noenc)) {
        (void)pst_decrypt(i_id, *buf, r, pf->encryption);
    }
    DEBUG_HEXDUMPC(*buf, r, 16);
    DEBUG_RET();
    return r;
}


/**
 * Read a block of data from file into memory
 * @param pf   PST file structure
 * @param i_id ID of block to read
 * @param buf  reference to pointer to buffer that will contain the data block.
 *             If this pointer is non-NULL, it will first be free()d.
 * @return     size of block read into memory
 */
static size_t pst_ff_getIDblock(pst_file *pf, uint64_t i_id, char** buf) {
    pst_index_ll *rec;
    size_t rsize;
    DEBUG_ENT("pst_ff_getIDblock");
    rec = pst_getID(pf, i_id);
    if (!rec) {
        DEBUG_INFO(("Cannot find ID %#"PRIx64"\n", i_id));
        DEBUG_RET();
        return 0;
    }
    DEBUG_INFO(("id = %#"PRIx64", record size = %#x, offset = %#x\n", i_id, rec->size, rec->offset));
    rsize = pst_read_block_size(pf, rec->offset, rec->size, buf);
    DEBUG_RET();
    return rsize;
}


static size_t pst_ff_getID2block(pst_file *pf, uint64_t id2, pst_id2_tree *id2_head, char** buf) {
    size_t ret;
    pst_id2_tree* ptr;
    pst_holder h = {buf, NULL, 0, 0, 0};
    DEBUG_ENT("pst_ff_getID2block");
    ptr = pst_getID2(id2_head, id2);

    if (!ptr) {
        DEBUG_WARN(("Cannot find id2 value %#"PRIx64"\n", id2));
        DEBUG_RET();
        return 0;
    }
    ret = pst_ff_getID2data(pf, ptr->id, &h);
    DEBUG_RET();
    return ret;
}


/** find the actual data from an i_id and send it to the destination
 *  specified by the pst_holder h. h must be a new empty destination.
 *
 *  @param pf     PST file structure
 *  @param ptr
 *  @param h      specifies the output destination (buffer, file, encoding)
 *  @return       updated size of the output
 */
static size_t pst_ff_getID2data(pst_file *pf, pst_index_ll *ptr, pst_holder *h) {
    size_t ret;
    char *b = NULL;
    DEBUG_ENT("pst_ff_getID2data");
    if (!(ptr->i_id & 0x02)) {
        ret = pst_ff_getIDblock_dec(pf, ptr->i_id, &b);
        ret = pst_append_holder(h, (size_t)0, &b, ret);
        free(b);
    } else {
        // here we will assume it is an indirection block that points to others
        DEBUG_INFO(("Assuming it is a multi-block record because of it's id %#"PRIx64"\n", ptr->i_id));
        ret = pst_ff_compile_ID(pf, ptr->i_id, h, (size_t)0);
    }
    ret = pst_finish_cleanup_holder(h, ret);
    DEBUG_RET();
    return ret;
}


/** find the actual data from an indirection i_id and send it to the destination
 *  specified by the pst_holder.
 *
 *  @param pf     PST file structure
 *  @param i_id   ID of the block to read
 *  @param h      specifies the output destination (buffer, file, encoding)
 *  @param size   number of bytes of data already sent to h
 *  @return       updated size of the output
 */
static size_t pst_ff_compile_ID(pst_file *pf, uint64_t i_id, pst_holder *h, size_t size) {
    size_t    z, a;
    uint16_t  count, y;
    char      *buf3 = NULL;
    char      *buf2 = NULL;
    char      *b_ptr;
    pst_block_hdr  block_hdr;
    pst_table3_rec table3_rec;  //for type 3 (0x0101) blocks

    DEBUG_ENT("pst_ff_compile_ID");
    a = pst_ff_getIDblock(pf, i_id, &buf3);
    if (!a) {
        if (buf3) free(buf3);
        DEBUG_RET();
        return 0;
    }
    DEBUG_HEXDUMPC(buf3, a, 16);
    memcpy(&block_hdr, buf3, sizeof(block_hdr));
    LE16_CPU(block_hdr.index_offset);
    LE16_CPU(block_hdr.type);
    LE32_CPU(block_hdr.offset);
    DEBUG_INFO(("block header (index_offset=%#hx, type=%#hx, offset=%#x)\n", block_hdr.index_offset, block_hdr.type, block_hdr.offset));

    count = block_hdr.type;
    b_ptr = buf3 + 8;

    // For indirect lookups through a table of i_ids, just recurse back into this
    // function, letting it concatenate all the data together, and then return the
    // total size of the data.
    if (block_hdr.index_offset == (uint16_t)0x0201) { // Indirect lookup (depth 2).
        for (y=0; y<count; y++) {
            b_ptr += pst_decode_type3(pf, &table3_rec, b_ptr);
            size = pst_ff_compile_ID(pf, table3_rec.id, h, size);
        }
        free(buf3);
        DEBUG_RET();
        return size;
    }

    if (block_hdr.index_offset != (uint16_t)0x0101) { //type 3
        DEBUG_WARN(("WARNING: not a type 0x0101 buffer, Treating as normal buffer\n"));
        if (pf->encryption) (void)pst_decrypt(i_id, buf3, a, pf->encryption);
        size = pst_append_holder(h, size, &buf3, a);
        free(buf3);
        DEBUG_RET();
        return size;
    }

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
        size = pst_append_holder(h, size, &buf2, z);
    }

    free(buf3);
    if (buf2) free(buf2);
    DEBUG_RET();
    return size;
}


/** append (buf,z) data to the output destination (h,size)
 *
 *  @param h      specifies the output destination (buffer, file, encoding)
 *  @param size   number of bytes of data already sent to h
 *  @param buf    reference to a pointer to the buffer to be appended to the destination
 *  @param z      number of bytes in buf
 *  @return       updated size of the output, buffer pointer possibly reallocated
 */
static size_t pst_append_holder(pst_holder *h, size_t size, char **buf, size_t z) {
    char *t;
    DEBUG_ENT("pst_append_holder");

    // raw append to a buffer
    if (h->buf) {
        *(h->buf) = pst_realloc(*(h->buf), size+z+1);
        DEBUG_INFO(("appending read data of size %i onto main buffer from pos %i\n", z, size));
        memcpy(*(h->buf)+size, *buf, z);

    // base64 encoding to a file
    } else if ((h->base64 == 1) && h->fp) {
        //
        if (h->base64_extra) {
            // include any bytes left over from the last encoding
            *buf = (char*)pst_realloc(*buf, z+h->base64_extra);
            memmove(*buf+h->base64_extra, *buf, z);
            memcpy(*buf, h->base64_extra_chars, h->base64_extra);
            z += h->base64_extra;
        }

        // find out how many bytes will be left over after this encoding and save them
        h->base64_extra = z % 3;
        if (h->base64_extra) {
            z -= h->base64_extra;
            memcpy(h->base64_extra_chars, *buf+z, h->base64_extra);
        }

        // encode this chunk
        t = pst_base64_encode_multiple(*buf, z, &h->base64_line_count);
        if (t) {
            DEBUG_INFO(("writing %i bytes to file as base64 [%i]. Currently %i\n", z, strlen(t), size));
            (void)pst_fwrite(t, (size_t)1, strlen(t), h->fp);
            free(t);    // caught by valgrind
        }

    // raw append to a file
    } else if (h->fp) {
        DEBUG_INFO(("writing %i bytes to file. Currently %i\n", z, size));
        (void)pst_fwrite(*buf, (size_t)1, z, h->fp);

    // null output
    } else {
        // h-> does not specify any output
    }
    DEBUG_RET();
    return size+z;
}


/** finish cleanup for base64 encoding to a file with extra bytes left over
 *
 *  @param h      specifies the output destination (buffer, file, encoding)
 *  @param size   number of bytes of data already sent to h
 *  @return       updated size of the output
 */
static size_t pst_finish_cleanup_holder(pst_holder *h, size_t size) {
    char *t;
    DEBUG_ENT("pst_finish_cleanup_holder");
    if ((h->base64 == 1) && h->fp && h->base64_extra) {
        // need to encode any bytes left over
        t = pst_base64_encode_multiple(h->base64_extra_chars, h->base64_extra, &h->base64_line_count);
        if (t) {
            (void)pst_fwrite(t, (size_t)1, strlen(t), h->fp);
            free(t);    // caught by valgrind
        }
        size += h->base64_extra;
    }
    DEBUG_RET();
    return size;
}


static int pst_stricmp(char *a, char *b) {
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


static int pst_strincmp(char *a, char *b, size_t x) {
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
    if (ptr)
        r = fwrite(ptr, size, nmemb, stream);
    else {
        r = 0;
        DEBUG_ENT("pst_fwrite");
        DEBUG_WARN(("An attempt to write a NULL Pointer was made\n"));
        DEBUG_RET();
    }
    return r;
}


static char* pst_wide_to_single(char *wt, size_t size) {
    // returns the first byte of each wide char. the size is the number of bytes in source
    char *x, *y;
    DEBUG_ENT("pst_wide_to_single");
    x = pst_malloc((size/2)+1);
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


char* pst_rfc2426_escape(char* str, char **buf, size_t* buflen) {
    //static char*  buf    = NULL;
    //static size_t buflen = 0;
    char *ret, *a, *b;
    size_t x = 0;
    int y, z;
    if (!str) return NULL;
    DEBUG_ENT("rfc2426_escape");
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
        if (x > *buflen) {
            *buf = (char*)pst_realloc(*buf, x);
            *buflen = x;
        }
        a = str;
        b = *buf;
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
        ret = *buf;
    }
    DEBUG_RET();
    return ret;
}


static int pst_chr_count(char *str, char x) {
    int r = 0;
    while (*str) {
        if (*str == x) r++;
        str++;
    }
    return r;
}


char* pst_rfc2425_datetime_format(const FILETIME* ft, int buflen, char* result) {
    struct tm stm;
    DEBUG_ENT("rfc2425_datetime_format");
    pst_fileTimeToStructTM(ft, &stm);
    if (strftime(result, buflen, "%Y-%m-%dT%H:%M:%SZ", &stm)==0) {
        DEBUG_INFO(("Problem occured formatting date\n"));
    }
    DEBUG_RET();
    return result;
}


char* pst_rfc2445_datetime_format(const FILETIME* ft, int buflen, char* result) {
    struct tm stm;
    DEBUG_ENT("rfc2445_datetime_format");
    pst_fileTimeToStructTM(ft, &stm);
    if (strftime(result, buflen, "%Y%m%dT%H%M%SZ", &stm)==0) {
        DEBUG_INFO(("Problem occured formatting date\n"));
    }
    DEBUG_RET();
    return result;
}


char* pst_rfc2445_datetime_format_now(int buflen, char* result) {
    struct tm stm;
    time_t t = time(NULL);
    DEBUG_ENT("rfc2445_datetime_format_now");
    gmtime_r(&t, &stm);
    if (strftime(result, buflen, "%Y%m%dT%H%M%SZ", &stm)==0) {
        DEBUG_INFO(("Problem occured formatting date\n"));
    }
    DEBUG_RET();
    return result;
}


/** Convert a code page integer into a string suitable for iconv()
 *
 *  @param cp the code page integer used in the pst file
 *  @param[in]  buflen  length of the output buffer
 *  @param[out] result  pointer to output buffer, must be at least 30 bytes
 *  @return pointer to a static buffer holding the string representation of the
 *          equivalent iconv character set
 */
static const char* codepage(int cp, int buflen, char* result);
static const char* codepage(int cp, int buflen, char* result) {
    switch (cp) {
        case   932 : return "iso-2022-jp";
        case   936 : return "gb2313";
        case   950 : return "big5";
        case  1200 : return "ucs-2le";
        case  1201 : return "ucs-2be";
        case 20127 : return "us-ascii";
        case 20269 : return "iso-6937";
        case 20865 : return "iso-8859-15";
        case 20866 : return "koi8-r";
        case 21866 : return "koi8-u";
        case 28591 : return "iso-8859-1";
        case 28592 : return "iso-8859-2";
        case 28595 : return "iso-8859-5";
        case 28596 : return "iso-8859-6";
        case 28597 : return "iso-8859-7";
        case 28598 : return "iso-8859-8";
        case 28599 : return "iso-8859-9";
        case 28600 : return "iso-8859-10";
        case 28601 : return "iso-8859-11";
        case 28602 : return "iso-8859-12";
        case 28603 : return "iso-8859-13";
        case 28604 : return "iso-8859-14";
        case 28605 : return "iso-8859-15";
        case 28606 : return "iso-8859-16";
        case 50220 : return "iso-2022-jp";
        case 50221 : return "csiso2022jp";
        case 51932 : return "euc-jp";
        case 51949 : return "euc-kr";
        case 65000 : return "utf-7";
        case 65001 : return "utf-8";
        default :
            snprintf(result, buflen, "windows-%d", cp);
            return result;
    }
    return NULL;
}


/** Get the default character set for this item. This is used to find
 *  the charset for pst_string elements that are not already in utf8 encoding.
 *
 *  @param  item   pointer to the mapi item of interest
 *  @param[in]  buflen  length of the output buffer
 *  @param[out] result  pointer to output buffer, must be at least 30 bytes
 *  @return default character set as a string useable by iconv()
 */
const char*    pst_default_charset(pst_item *item, int buflen, char* result) {
    return (item->body_charset.str)         ? item->body_charset.str :
           (item->message_codepage)         ? codepage(item->message_codepage, buflen, result) :
           (item->internet_cpid)            ? codepage(item->internet_cpid, buflen, result) :
           (item->pf && item->pf->charset)  ? item->pf->charset :
           "iso-8859-1";
}


/** Convert str to rfc2231 encoding of str
 *
 *  @param str   pointer to the mapi string of interest
 */
void pst_rfc2231(pst_string *str) {
    int needs = 0;
    const int8_t *x = (int8_t *)str->str;
    while (*x) {
        if (*x <= 32) needs++;
        x++;
    }
    int n = strlen(str->str) + 2*needs + 15;
    char *buffer = pst_malloc(n);
    strcpy(buffer, "utf-8''");
    x = (int8_t *)str->str;
    const uint8_t *y = (uint8_t *)str->str;
    uint8_t *z = (uint8_t *)buffer;
    z += strlen(buffer);    // skip the utf8 prefix
    while (*y) {
        if (*x <= 32) {
            *(z++) = (uint8_t)'%';
            snprintf(z, 3, "%2x", *y);
            z += 2;
        }
        else {
            *(z++) = *y;
        }
        x++;
        y++;
    }
    *z = '\0';
    free(str->str);
    str->str = buffer;
}


/** Convert str to rfc2047 encoding of str, possibly enclosed in quotes if it contains spaces
 *
 *  @param item          pointer to the containing mapi item
 *  @param str           pointer to the mapi string of interest
 *  @param needs_quote   true if strings containing spaces should be wrapped in quotes
 */
void pst_rfc2047(pst_item *item, pst_string *str, int needs_quote) {
    int has_space = 0;
    int needs_coding = 0;
    pst_convert_utf8(item, str);
    const int8_t *x = (int8_t *)str->str;
    while (*x) {
        if (*x == 32) has_space = 1;
        if (*x < 32)  needs_coding = 1;
        x++;
    }
    if (needs_coding) {
        char *enc = pst_base64_encode_single(str->str, strlen(str->str));
        free(str->str);
        int n = strlen(enc) + 20;
        str->str = pst_malloc(n);
        snprintf(str->str, n, "=?utf-8?B?%s?=", enc);
        free(enc);
    }
    else if (has_space && needs_quote) {
        int n = strlen(str->str) + 10;
        char *buffer = pst_malloc(n);
        snprintf(buffer, n, "\"%s\"", str->str);
        free(str->str);
        str->str = buffer;
    }
}


/** Convert str to utf8 if possible; null strings are preserved.
 *
 *  @param item  pointer to the containing mapi item
 *  @param str   pointer to the mapi string of interest
 */
void pst_convert_utf8_null(pst_item *item, pst_string *str) {
    if (!str->str) return;
    pst_convert_utf8(item, str);
}


/** Convert str to utf8 if possible; null strings are converted into empty strings.
 *
 *  @param item  pointer to the containing mapi item
 *  @param str   pointer to the mapi string of interest
 */
void pst_convert_utf8(pst_item *item, pst_string *str) {
    DEBUG_ENT("pst_convert_utf8");
    char buffer[30];
    if (str->is_utf8) {
        DEBUG_WARN(("Already utf8\n"));
        DEBUG_RET();
        return;
    }
    if (!str->str) {
        str->str = strdup("");
        DEBUG_WARN(("null to empty string\n"));
        DEBUG_RET();
        return;
    }
    const char *charset = pst_default_charset(item, sizeof(buffer), buffer);
    DEBUG_WARN(("default charset is %s\n", charset));
    if (!strcasecmp("utf-8", charset)) {
        DEBUG_RET();
        return;
    }
    pst_vbuf *newer = pst_vballoc(2);
    size_t rc = pst_vb_8bit2utf8(newer, str->str, strlen(str->str) + 1, charset);
    if (rc == (size_t)-1) {
        free(newer->b);
        DEBUG_WARN(("Failed to convert %s to utf-8 - %s\n", charset, str->str));
    }
    else {
        free(str->str);
        str->str = newer->b;
        str->is_utf8 = 1;
    }
    free(newer);
    DEBUG_RET();
}


/** Decode raw recurrence data into a better structure.
 * @param appt pointer to appointment structure
 * @return     pointer to decoded recurrence structure that must be free'd by the caller.
 */
pst_recurrence* pst_convert_recurrence(pst_item_appointment* appt)
{
    const int bias = 30 * 24 * 60;  // minutes in 30 days
    int m[4] = {3,4,4,5};
    pst_recurrence *r = pst_malloc(sizeof(pst_recurrence));
    memset(r, 0, sizeof(pst_recurrence));
    size_t s = appt->recurrence_data.size;
    size_t i = 0;
    char*  p = appt->recurrence_data.data;
    if (p) {
        if (i+4 <= s) { r->signature        = PST_LE_GET_UINT32(p+i);        i += 4; }
        if (i   <= s) { r->type             = PST_LE_GET_UINT8(p+i) - 0x0a;  i += 2; }
        if (i+4 <= s) { r->sub_type         = PST_LE_GET_UINT32(p+i);        i += 4; }
        if (r->sub_type <= 3) {
            int n = m[r->sub_type]; // number of parms for this sub_type
            int j = 0;
            for (j=0; j<n; j++) {
                if (i+4 <= s) { *(&r->parm1 + j) = PST_LE_GET_UINT32(p+i);   i += 4; }
            }
        }
        if (i   <= s) { r->termination      = PST_LE_GET_UINT8(p+i) - 0x21;  i += 4; }
        if (i+4 <= s) { r->count            = PST_LE_GET_UINT32(p+i);        i += 4; }
        if (r->termination == 2) r->count = 0;
        switch (r->type) {
            case 0: // daily
                if (r->sub_type == 0) {
                    // simple daily
                    r->interval = r->parm2 / (24 * 60); // was minutes between recurrences
                }
                else {
                    // daily every weekday, subset of weekly
                    r->interval  = 1;
                    r->bydaymask = r->parm4;
                }
                break;
            case 1: // weekly
                r->interval  = r->parm2;
                r->bydaymask = r->parm4;
                break;
            case 2: // monthly
                r->interval = r->parm2;
                if (r->sub_type == 2) {
                    // monthly on day d
                    r->dayofmonth = r->parm4;
                }
                else {
                    // monthly on 2nd tuesday
                    r->bydaymask = r->parm4;
                    r->position  = r->parm5;
                }
                break;
            case 3: // yearly
                r->interval    = 1;
                r->monthofyear = ((r->parm1 + bias/2) / bias) + 1;
                if (r->sub_type == 2) {
                    // yearly on day d of month m
                    r->dayofmonth  = r->parm4;
                }
                else {
                    // yearly on 2nd tuesday of month m
                    r->bydaymask = r->parm4;
                    r->position  = r->parm5;
                }
                break;
            default:
                break;
        }
    }
    return r;
}


/** Free a recurrence structure.
 * @param r input pointer to be freed
 */
void pst_free_recurrence(pst_recurrence* r)
{
    if (r) free(r);
}
