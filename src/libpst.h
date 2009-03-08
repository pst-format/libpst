/***
 * libpst.h
 * Part of LibPST project
 * Written by David Smith
 *            dave.s@earthcorp.com
 */
// LibPST - Library for Accessing Outlook .pst files
// Dave Smith - davesmith@users.sourceforge.net

#ifndef LIBPST_H
#define LIBPST_H

#include "common.h"


// switch to maximal packing for all structures in the libpst interface
// this is reverted at the end of this file
#ifdef _MSC_VER
    #pragma pack(push, 1)
#endif
#if defined(__GNUC__) || defined (__SUNPRO_C) || defined(__SUNPRO_CC)
    #pragma pack(1)
#endif


#define PST_TYPE_NOTE        1
#define PST_TYPE_APPOINTMENT 8
#define PST_TYPE_CONTACT     9
#define PST_TYPE_JOURNAL    10
#define PST_TYPE_STICKYNOTE 11
#define PST_TYPE_TASK       12
#define PST_TYPE_OTHER      13
#define PST_TYPE_REPORT     14

// defines whether decryption is done on this bit of data
#define PST_NO_ENC 0
#define PST_ENC    1

// defines types of possible encryption
#define PST_NO_ENCRYPT   0
#define PST_COMP_ENCRYPT 1
#define PST_ENCRYPT      2

// defines different types of mappings
#define PST_MAP_ATTRIB (uint32_t)1
#define PST_MAP_HEADER (uint32_t)2

// define my custom email attributes.
#define PST_ATTRIB_HEADER -1

// defines types of free/busy values for appointment->showas
#define PST_FREEBUSY_FREE 0
#define PST_FREEBUSY_TENTATIVE 1
#define PST_FREEBUSY_BUSY 2
#define PST_FREEBUSY_OUT_OF_OFFICE 3

// defines labels for appointment->label
#define PST_APP_LABEL_NONE        0 // None
#define PST_APP_LABEL_IMPORTANT   1 // Important
#define PST_APP_LABEL_BUSINESS    2 // Business
#define PST_APP_LABEL_PERSONAL    3 // Personal
#define PST_APP_LABEL_VACATION    4 // Vacation
#define PST_APP_LABEL_MUST_ATTEND 5 // Must Attend
#define PST_APP_LABEL_TRAVEL_REQ  6 // Travel Required
#define PST_APP_LABEL_NEEDS_PREP  7 // Needs Preparation
#define PST_APP_LABEL_BIRTHDAY    8 // Birthday
#define PST_APP_LABEL_ANNIVERSARY 9 // Anniversary
#define PST_APP_LABEL_PHONE_CALL  10// Phone Call

// define type of reccuring event
#define PST_APP_RECUR_NONE        0
#define PST_APP_RECUR_DAILY       1
#define PST_APP_RECUR_WEEKLY      2
#define PST_APP_RECUR_MONTHLY     3
#define PST_APP_RECUR_YEARLY      4


typedef struct pst_misc_6_struct {
    int32_t i1;
    int32_t i2;
    int32_t i3;
    int32_t i4;
    int32_t i5;
    int32_t i6;
} pst_misc_6;


typedef struct pst_entryid_struct {
    int32_t u1;
    char entryid[16];
    uint32_t id;
} pst_entryid;


typedef struct pst_desc_struct32 {
    uint32_t d_id;
    uint32_t desc_id;
    uint32_t tree_id;
    uint32_t parent_d_id;
} pst_desc32;


typedef struct pst_desc_structn {
    uint64_t d_id;
    uint64_t desc_id;
    uint64_t tree_id;
    uint32_t parent_d_id;   // not 64 bit ??
    uint32_t u1;            // padding
} pst_descn;


typedef struct pst_index_struct32 {
    uint32_t id;
    uint32_t offset;
    uint16_t size;
    int16_t  u1;
} pst_index32;


typedef struct pst_index_struct {
    uint64_t id;
    uint64_t offset;
    uint16_t size;
    int16_t  u0;
    int32_t  u1;
} pst_index;


typedef struct pst_index_tree32 {
    uint32_t id;
    uint32_t offset;
    uint32_t size;
    int32_t  u1;
    struct pst_index_tree * next;
} pst_index_ll32;


typedef struct pst_index_tree {
    uint64_t id;
    uint64_t offset;
    uint64_t size;
    int64_t  u1;
    struct pst_index_tree *next;
} pst_index_ll;


typedef struct pst_id2_tree {
    uint64_t id2;
    pst_index_ll *id;
    struct pst_id2_tree *child;
    struct pst_id2_tree *next;
} pst_id2_ll;


typedef struct pst_desc_tree {
    uint64_t              d_id;
    uint64_t              parent_d_id;
    pst_index_ll         *desc;
    pst_index_ll         *assoc_tree;
    int32_t               no_child;
    struct pst_desc_tree *prev;
    struct pst_desc_tree *next;
    struct pst_desc_tree *parent;
    struct pst_desc_tree *child;
    struct pst_desc_tree *child_tail;
} pst_desc_ll;


typedef struct pst_string {
    int     is_utf8;    // 1 = true, 0 = false
    char   *str;        // either utf8 or some sbcs
} pst_string;


typedef struct pst_item_email {
    FILETIME   *arrival_date;
    int         autoforward;            // 1 = true, 0 = not set, -1 = false
    pst_string  cc_address;
    pst_string  bcc_address;
    int32_t     conv_index;
    int         conversion_prohibited;  // 1 = true, 0 = false
    int         delete_after_submit;    // 1 = true, 0 = false
    int         delivery_report;        // 1 = true, 0 = false
    char       *encrypted_body;
    size_t      encrypted_body_size;
    char       *encrypted_htmlbody;
    size_t      encrypted_htmlbody_size;
    int32_t     flag;
    pst_string  header;
    pst_string  htmlbody;
    int32_t     importance;
    pst_string  in_reply_to;
    int         message_cc_me;          // 1 = true, 0 = false
    int         message_recip_me;       // 1 = true, 0 = false
    int         message_to_me;          // 1 = true, 0 = false
    pst_string  messageid;
    int32_t     orig_sensitivity;
    pst_string  original_bcc;
    pst_string  original_cc;
    pst_string  original_to;
    pst_string  outlook_recipient;
    pst_string  outlook_recipient_name;
    pst_string  outlook_recipient2;
    pst_string  outlook_sender;
    pst_string  outlook_sender_name;
    pst_string  outlook_sender2;
    int32_t     priority;
    pst_string  processed_subject;
    int         read_receipt;           // 1 = true, 0 = false
    pst_string  recip_access;
    pst_string  recip_address;
    pst_string  recip2_access;
    pst_string  recip2_address;
    int         reply_requested;        // 1 = true, 0 = false
    pst_string  reply_to;
    pst_string  return_path_address;
    int32_t     rtf_body_char_count;
    int32_t     rtf_body_crc;
    pst_string  rtf_body_tag;
    char       *rtf_compressed;
    uint32_t    rtf_compressed_size;
    int         rtf_in_sync;            // 1 = true, 0 = doesn't exist, -1 = false
    int32_t     rtf_ws_prefix_count;
    int32_t     rtf_ws_trailing_count;
    pst_string  sender_access;
    pst_string  sender_address;
    pst_string  sender2_access;
    pst_string  sender2_address;
    int32_t     sensitivity;
    FILETIME    *sent_date;
    pst_entryid *sentmail_folder;
    pst_string  sentto_address;
    // delivery report fields
    pst_string  report_text;
    FILETIME   *report_time;
    int32_t     ndr_reason_code;
    int32_t     ndr_diag_code;
    pst_string  supplementary_info;
    int32_t     ndr_status_code;
} pst_item_email;


typedef struct pst_item_folder {
    int32_t  email_count;
    int32_t  unseen_email_count;
    int32_t  assoc_count;
    int      subfolder;               // 1 = true, 0 = false
} pst_item_folder;


typedef struct pst_item_message_store {
    pst_entryid *top_of_personal_folder;        // 0x35e0
    pst_entryid *default_outbox_folder;         // 0x35e2
    pst_entryid *deleted_items_folder;          // 0x35e3
    pst_entryid *sent_items_folder;             // 0x35e4
    pst_entryid *user_views_folder;             // 0x35e5
    pst_entryid *common_view_folder;            // 0x35e6
    pst_entryid *search_root_folder;            // 0x35e7
    pst_entryid *top_of_folder;                 // 0x7c07
    int32_t valid_mask;                         // 0x35df  // what folders the message store contains
    int32_t pwd_chksum;                         // 0x76ff
} pst_item_message_store;


typedef struct pst_item_contact {
    pst_string  access_method;
    pst_string  account_name;
    pst_string  address1;
    pst_string  address1a;
    pst_string  address1_desc;
    pst_string  address1_transport;
    pst_string  address2;
    pst_string  address2a;
    pst_string  address2_desc;
    pst_string  address2_transport;
    pst_string  address3;
    pst_string  address3a;
    pst_string  address3_desc;
    pst_string  address3_transport;
    pst_string  assistant_name;
    pst_string  assistant_phone;
    pst_string  billing_information;
    FILETIME   *birthday;
    pst_string  business_address;               // 0x801b
    pst_string  business_city;
    pst_string  business_country;
    pst_string  business_fax;
    pst_string  business_homepage;
    pst_string  business_phone;
    pst_string  business_phone2;
    pst_string  business_po_box;
    pst_string  business_postal_code;
    pst_string  business_state;
    pst_string  business_street;
    pst_string  callback_phone;
    pst_string  car_phone;
    pst_string  company_main_phone;
    pst_string  company_name;
    pst_string  computer_name;
    pst_string  customer_id;
    pst_string  def_postal_address;
    pst_string  department;
    pst_string  display_name_prefix;
    pst_string  first_name;
    pst_string  followup;
    pst_string  free_busy_address;
    pst_string  ftp_site;
    pst_string  fullname;
    int16_t     gender;
    pst_string  gov_id;
    pst_string  hobbies;
    pst_string  home_address;                   // 0x801a
    pst_string  home_city;
    pst_string  home_country;
    pst_string  home_fax;
    pst_string  home_phone;
    pst_string  home_phone2;
    pst_string  home_po_box;
    pst_string  home_postal_code;
    pst_string  home_state;
    pst_string  home_street;
    pst_string  initials;
    pst_string  isdn_phone;
    pst_string  job_title;
    pst_string  keyword;
    pst_string  language;
    pst_string  location;
    int         mail_permission;                // 1 = true, 0 = false
    pst_string  manager_name;
    pst_string  middle_name;
    pst_string  mileage;
    pst_string  mobile_phone;
    pst_string  nickname;
    pst_string  office_loc;
    pst_string  common_name;
    pst_string  org_id;
    pst_string  other_address;                  // 0x801c
    pst_string  other_city;
    pst_string  other_country;
    pst_string  other_phone;
    pst_string  other_po_box;
    pst_string  other_postal_code;
    pst_string  other_state;
    pst_string  other_street;
    pst_string  pager_phone;
    pst_string  personal_homepage;
    pst_string  pref_name;
    pst_string  primary_fax;
    pst_string  primary_phone;
    pst_string  profession;
    pst_string  radio_phone;
    int         rich_text;                      // 1 = true, 0 = false
    pst_string  spouse_name;
    pst_string  suffix;
    pst_string  surname;
    pst_string  telex;
    pst_string  transmittable_display_name;
    pst_string  ttytdd_phone;
    FILETIME   *wedding_anniversary;
    pst_string  work_address_street;            // 0x8045
    pst_string  work_address_city;              // 0x8046
    pst_string  work_address_state;             // 0x8047
    pst_string  work_address_postalcode;        // 0x8048
    pst_string  work_address_country;           // 0x8049
    pst_string  work_address_postofficebox;     // 0x804a
} pst_item_contact;


typedef struct pst_item_attach {
    pst_string  filename1;
    pst_string  filename2;
    pst_string  mimetype;
    char       *data;
    size_t      size;
    uint64_t    id2_val;
    uint64_t    id_val;     // calculated from id2_val during creation of record
    pst_id2_ll *id2_head;   // deep copy from child
    int32_t     method;
    int32_t     position;
    int32_t     sequence;
    struct pst_item_attach *next;
} pst_item_attach;


typedef struct pst_item_extra_field {
    char   *field_name;
    char   *value;
    struct pst_item_extra_field *next;
} pst_item_extra_field;


typedef struct pst_item_journal {
    FILETIME   *end;
    FILETIME   *start;
    pst_string  type;
} pst_item_journal;


typedef struct pst_item_appointment {
    FILETIME   *end;
    pst_string  location;
    int         alarm;              // 1 = true, 0 = false
    FILETIME   *reminder;
    int32_t     alarm_minutes;
    pst_string  alarm_filename;
    FILETIME   *start;
    pst_string  timezonestring;
    int32_t     showas;
    int32_t     label;
    int         all_day;            // 1 = true, 0 = false
    pst_string  recurrence;
    int32_t     recurrence_type;
    FILETIME   *recurrence_start;
    FILETIME   *recurrence_end;
} pst_item_appointment;


typedef struct pst_item {
    struct pst_item_email         *email;           // data referring to email
    struct pst_item_folder        *folder;          // data referring to folder
    struct pst_item_contact       *contact;         // data referring to contact
    struct pst_item_attach        *attach;          // linked list of attachments
    struct pst_item_message_store *message_store;   // data referring to the message store
    struct pst_item_extra_field   *extra_fields;    // linked list of extra headers and such
    struct pst_item_journal       *journal;         // data referring to a journal entry
    struct pst_item_appointment   *appointment;     // data referring to a calendar entry
    int         type;
    char       *ascii_type;
    pst_string  file_as;
    pst_string  comment;
    pst_string  body_charset;           // null if not specified
    pst_string  body;                   // email, journal
    pst_string  subject;                // email, journal
    int32_t     internet_cpid;
    int32_t     message_codepage;
    int32_t     message_size;
    pst_string  outlook_version;
    char       *record_key;             // probably 16 bytes long.
    size_t      record_key_size;
    int         response_requested;     // 1 = true, 0 = false
    FILETIME   *create_date;
    FILETIME   *modify_date;
    int         private_member;         // 1 = true, 0 = false
} pst_item;


typedef struct pst_x_attrib_ll {
    uint32_t type;
    uint32_t mytype;
    uint32_t map;
    void *data;
    struct pst_x_attrib_ll *next;
} pst_x_attrib_ll;


typedef struct pst_block_recorder {
    struct pst_block_recorder  *next;
    int64_t                     offset;
    size_t                      size;
    int                         readcount;
} pst_block_recorder;


typedef struct pst_file {
    pst_index_ll *i_head, *i_tail;
    pst_desc_ll  *d_head, *d_tail;
    pst_x_attrib_ll *x_head;
    pst_block_recorder *block_head;

    int do_read64;              // 0 is 32-bit pst file, pre Outlook 2003;
                                // 1 is 64-bit pst file, Outlook 2003 and later
    uint64_t index1;
    uint64_t index1_back;
    uint64_t index2;
    uint64_t index2_back;
    FILE * fp;                  // file pointer to opened PST file
    uint64_t size;              // pst file size
    unsigned char encryption;   // pst encryption setting
    unsigned char ind_type;     // pst index type
} pst_file;


typedef struct pst_block_offset {
    int16_t from;
    int16_t to;
} pst_block_offset;


typedef struct pst_block_offset_pointer {
    char *from;
    char *to;
    int   needfree;
} pst_block_offset_pointer;


typedef struct pst_num_item {
    uint32_t   id;      // not an id1 or id2, this is actually some sort of type code
    char      *data;
    uint32_t   type;
    size_t     size;
    char      *extra;
} pst_num_item;


typedef struct pst_num_array {
    int32_t count_item;
    int32_t orig_count;
    int32_t count_array;
    struct pst_num_item ** items;
    struct pst_num_array *next;
} pst_num_array;


typedef struct pst_holder {
    char  **buf;
    FILE   *fp;
    int     base64;
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


// prototypes
int            pst_open(pst_file *pf, char *name);
int            pst_close(pst_file *pf);
pst_desc_ll *  pst_getTopOfFolders(pst_file *pf, pst_item *root);
size_t         pst_attach_to_mem(pst_file *pf, pst_item_attach *attach, char **b);
size_t         pst_attach_to_file(pst_file *pf, pst_item_attach *attach, FILE* fp);
size_t         pst_attach_to_file_base64(pst_file *pf, pst_item_attach *attach, FILE* fp);
int            pst_load_index (pst_file *pf);
pst_desc_ll*   pst_getNextDptr(pst_desc_ll* d);
int            pst_load_extended_attributes(pst_file *pf);

int            pst_build_id_ptr(pst_file *pf, int64_t offset, int32_t depth, uint64_t linku1, uint64_t start_val, uint64_t end_val);
int            pst_build_desc_ptr(pst_file *pf, int64_t offset, int32_t depth, uint64_t linku1, uint64_t start_val, uint64_t end_val);
pst_item*      pst_getItem(pst_file *pf, pst_desc_ll *d_ptr);
pst_item*      pst_parse_item (pst_file *pf, pst_desc_ll *d_ptr, pst_id2_ll *m_head);
pst_num_array* pst_parse_block(pst_file *pf, uint64_t block_id, pst_id2_ll *i2_head, pst_num_array *na_head);
int            pst_process(pst_num_array *list, pst_item *item, pst_item_attach *attach);
void           pst_free_list(pst_num_array *list);
void           pst_freeItem(pst_item *item);
void           pst_free_id2(pst_id2_ll * head);
void           pst_free_id (pst_index_ll *head);
void           pst_free_desc (pst_desc_ll *head);
void           pst_free_xattrib(pst_x_attrib_ll *x);
int            pst_getBlockOffsetPointer(pst_file *pf, pst_id2_ll *i2_head, pst_subblocks *subblocks, uint32_t offset, pst_block_offset_pointer *p);
int            pst_getBlockOffset(char *buf, size_t read_size, uint32_t i_offset, uint32_t offset, pst_block_offset *p);
pst_id2_ll* pst_build_id2(pst_file *pf, pst_index_ll* list);
pst_index_ll*  pst_getID(pst_file* pf, uint64_t id);
pst_id2_ll* pst_getID2(pst_id2_ll * ptr, uint64_t id);
pst_desc_ll*   pst_getDptr(pst_file *pf, uint64_t id);
size_t         pst_read_block_size(pst_file *pf, int64_t offset, size_t size, char **buf);
int            pst_decrypt(uint64_t id, char *buf, size_t size, unsigned char type);
uint64_t       pst_getIntAt(pst_file *pf, char *buf);
uint64_t       pst_getIntAtPos(pst_file *pf, int64_t pos);
size_t         pst_getAtPos(pst_file *pf, int64_t pos, void* buf, size_t size);
size_t         pst_ff_getIDblock_dec(pst_file *pf, uint64_t id, char **b);
size_t         pst_ff_getIDblock(pst_file *pf, uint64_t id, char** b);
size_t         pst_ff_getID2block(pst_file *pf, uint64_t id2, pst_id2_ll *id2_head, char** buf);
size_t         pst_ff_getID2data(pst_file *pf, pst_index_ll *ptr, pst_holder *h);
size_t         pst_ff_compile_ID(pst_file *pf, uint64_t id, pst_holder *h, size_t size);

int            pst_strincmp(char *a, char *b, size_t x);
int            pst_stricmp(char *a, char *b);
size_t         pst_fwrite(const void*ptr, size_t size, size_t nmemb, FILE*stream);
char *         pst_wide_to_single(char *wt, size_t size);

char *         pst_rfc2426_escape(char *str);
int            pst_chr_count(char *str, char x);
char *         pst_rfc2425_datetime_format(FILETIME *ft);
char *         pst_rfc2445_datetime_format(FILETIME *ft);

void           pst_printDptr(pst_file *pf, pst_desc_ll *ptr);
void           pst_printIDptr(pst_file* pf);
void           pst_printID2ptr(pst_id2_ll *ptr);

const char*    pst_codepage(int cp);
const char*    pst_default_charset(pst_item *item);
void           pst_convert_utf8_null(pst_item *item, pst_string *str);
void           pst_convert_utf8(pst_item *item, pst_string *str);


// switch from maximal packing back to default packing
// undo the packing from the beginning of this file
#ifdef _MSC_VER
    #pragma pack(pop)
#endif
#if defined(__GNUC__) || defined (__SUNPRO_C) || defined(__SUNPRO_CC)
    #pragma pack()
#endif



#endif // defined LIBPST_H
