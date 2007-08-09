/***
 * libpst.h
 * Part of LibPST project
 * Written by David Smith
 *			  dave.s@earthcorp.com
 */
// LibPST - Library for Accessing Outlook .pst files
// Dave Smith - davesmith@users.sourceforge.net

#ifndef LIBPST_H
#define LIBPST_H

#ifndef  _MSC_VER

#ifndef FILETIME_DEFINED
#define FILETIME_DEFINED
//Win32 Filetime struct - copied from WINE
typedef struct {
  u_int32_t dwLowDateTime;
  u_int32_t dwHighDateTime;
} FILETIME;
#endif //ifndef FILETIME_DEFINED
#endif //ifndef  _MSC_VER

// define the INT32_MAX here cause it isn't normally defined
#ifndef INT32_MAX
# define INT32_MAX INT_MAX
#endif

// According to Jan Wolter, sys/param.h is the most portable source of endian
// information on UNIX systems. see http://www.unixpapa.com/incnote/byteorder.html
#ifdef _MSC_VER
  #define BYTE_ORDER LITTLE_ENDIAN
#else
  #include <sys/param.h>
#endif // defined _MSC_VER

#if BYTE_ORDER == BIG_ENDIAN
#  define LE64_CPU(x) \
  x = ((((x) & 0xff00000000000000) >> 56) | \
	   (((x) & 0x00ff000000000000) >> 40) | \
	   (((x) & 0x0000ff0000000000) >> 24) | \
	   (((x) & 0x000000ff00000000) >> 8 ) | \
	   (((x) & 0x00000000ff000000) << 8 ) | \
	   (((x) & 0x0000000000ff0000) << 24) | \
	   (((x) & 0x000000000000ff00) << 40) | \
	   (((x) & 0x00000000000000ff) << 56));
#  define LE32_CPU(x) \
  x = ((((x) & 0xff000000) >> 24) | \
	   (((x) & 0x00ff0000) >> 8 ) | \
	   (((x) & 0x0000ff00) << 8 ) | \
	   (((x) & 0x000000ff) << 24));
#  define LE16_CPU(x) \
  x = ((((x) & 0xff00) >> 8) | \
	   (((x) & 0x00ff) << 8));
#elif BYTE_ORDER == LITTLE_ENDIAN
#  define LE64_CPU(x) {}
#  define LE32_CPU(x) {}
#  define LE16_CPU(x) {}
#else
#  error "Byte order not supported by this library"
#endif // BYTE_ORDER


#ifdef _MSC_VER
#include "windows.h"
#define int32_t int
#define u_int32_t unsigned int
#define int16_t short int
#define u_int16_t unsigned short int
#endif // _MSC_VER

#define PST_TYPE_NOTE 1
#define PST_TYPE_APPOINTMENT 8
#define PST_TYPE_CONTACT 9
#define PST_TYPE_JOURNAL 10
#define PST_TYPE_STICKYNOTE 11
#define PST_TYPE_TASK 12
#define PST_TYPE_OTHER 13
#define PST_TYPE_REPORT 14

// defines whether decryption is done on this bit of data
#define PST_NO_ENC 0
#define PST_ENC 1

// defines types of possible encryption
#define PST_NO_ENCRYPT 0
#define PST_COMP_ENCRYPT 1
#define PST_ENCRYPT 2

// defines different types of mappings
#define PST_MAP_ATTRIB 1
#define PST_MAP_HEADER 2

// define my custom email attributes.
#define PST_ATTRIB_HEADER -1

// defines types of free/busy values for appointment->showas
#define PST_FREEBUSY_FREE 0
#define PST_FREEBUSY_TENTATIVE 1
#define PST_FREEBUSY_BUSY 2
#define PST_FREEBUSY_OUT_OF_OFFICE 3

// defines labels for appointment->label
#define PST_APP_LABEL_NONE		  0 // None
#define PST_APP_LABEL_IMPORTANT   1 // Important
#define PST_APP_LABEL_BUSINESS	  2 // Business
#define PST_APP_LABEL_PERSONAL	  3 // Personal
#define PST_APP_LABEL_VACATION	  4 // Vacation
#define PST_APP_LABEL_MUST_ATTEND 5 // Must Attend
#define PST_APP_LABEL_TRAVEL_REQ  6 // Travel Required
#define PST_APP_LABEL_NEEDS_PREP  7 // Needs Preparation
#define PST_APP_LABEL_BIRTHDAY	  8 // Birthday
#define PST_APP_LABEL_ANNIVERSARY 9 // Anniversary
#define PST_APP_LABEL_PHONE_CALL  10// Phone Call

typedef struct _pst_misc_6_struct {
  int32_t i1;
  int32_t i2;
  int32_t i3;
  int32_t i4;
  int32_t i5;
  int32_t i6;
} pst_misc_6;

typedef struct _pst_entryid_struct {
  int32_t u1;
  char entryid[16];
  u_int32_t id;
} pst_entryid;

typedef struct _pst_desc_struct {
  u_int32_t d_id;
  u_int32_t desc_id;
  u_int32_t list_id;
  u_int32_t parent_id;
} pst_desc;

typedef struct _pst_index_struct{
  u_int32_t id;
  int32_t offset;
  u_int16_t size;
  int16_t u1;
} pst_index;

typedef struct _pst_index_tree {
  u_int32_t id;
  int32_t offset;
  size_t size;
  int32_t u1;
  struct _pst_index_tree * next;
} pst_index_ll;

typedef struct _pst_index2_tree {
  u_int32_t id2;
  pst_index_ll *id;
  struct _pst_index2_tree * next;
} pst_index2_ll;

typedef struct _pst_desc_tree {
  u_int32_t id;
  pst_index_ll * list_index;
  pst_index_ll * desc;
  int32_t no_child;
  struct _pst_desc_tree * prev;
  struct _pst_desc_tree * next;
  struct _pst_desc_tree * parent;
  struct _pst_desc_tree * child;
  struct _pst_desc_tree * child_tail;
} pst_desc_ll;

typedef struct _pst_item_email_subject {
  int32_t off1;
  int32_t off2;
  char *subj;
} pst_item_email_subject;

typedef struct _pst_item_email {
  FILETIME *arrival_date;
  int32_t autoforward; // 1 = true, 0 = not set, -1 = false
  char *body;
  char *cc_address;
  char *common_name;
  int32_t  conv_index;
  int32_t  conversion_prohib;
  int32_t  delete_after_submit; // 1 = true, 0 = false
  int32_t  delivery_report; // 1 = true, 0 = false
  char *encrypted_body;
  int32_t  encrypted_body_size;
  char *encrypted_htmlbody;
  int32_t encrypted_htmlbody_size;
  int32_t  flag;
  char *header;
  char *htmlbody;
  int32_t  importance;
  char *in_reply_to;
  int32_t  message_cc_me; // 1 = true, 0 = false
  int32_t  message_recip_me; // 1 = true, 0 = false
  int32_t  message_to_me; // 1 = true, 0 = false
  char *messageid;
  int32_t  orig_sensitivity;
  char *outlook_recipient;
  char *outlook_recipient2;
  char *outlook_sender;
  char *outlook_sender_name;
  char *outlook_sender2;
  int32_t  priority;
  char *proc_subject;
  int32_t  read_receipt;
  char *recip_access;
  char *recip_address;
  char *recip2_access;
  char *recip2_address;
  int32_t  reply_requested;
  char *reply_to;
  char *return_path_address;
  int32_t  rtf_body_char_count;
  int32_t  rtf_body_crc;
  char *rtf_body_tag;
  char *rtf_compressed;
  int32_t  rtf_in_sync; // 1 = true, 0 = doesn't exist, -1 = false
  int32_t  rtf_ws_prefix_count;
  int32_t  rtf_ws_trailing_count;
  char *sender_access;
  char *sender_address;
  char *sender2_access;
  char *sender2_address;
  int32_t  sensitivity;
  FILETIME *sent_date;
  pst_entryid *sentmail_folder;
  char *sentto_address;
  pst_item_email_subject *subject;
} pst_item_email;

typedef struct _pst_item_folder {
  int32_t  email_count;
  int32_t  unseen_email_count;
  int32_t  assoc_count;
  char subfolder;
} pst_item_folder;

typedef struct _pst_item_message_store {
  pst_entryid *deleted_items_folder;
  pst_entryid *search_root_folder;
  pst_entryid *top_of_personal_folder;
  pst_entryid *top_of_folder;
  int32_t valid_mask; // what folders the message store contains
  int32_t pwd_chksum;
} pst_item_message_store;

typedef struct _pst_item_contact {
  char *access_method;
  char *account_name;
  char *address1;
  char *address1a;
  char *address1_desc;
  char *address1_transport;
  char *address2;
  char *address2a;
  char *address2_desc;
  char *address2_transport;
  char *address3;
  char *address3a;
  char *address3_desc;
  char *address3_transport;
  char *assistant_name;
  char *assistant_phone;
  char *billing_information;
  FILETIME *birthday;
  char *business_address;
  char *business_city;
  char *business_country;
  char *business_fax;
  char *business_homepage;
  char *business_phone;
  char *business_phone2;
  char *business_po_box;
  char *business_postal_code;
  char *business_state;
  char *business_street;
  char *callback_phone;
  char *car_phone;
  char *company_main_phone;
  char *company_name;
  char *computer_name;
  char *customer_id;
  char *def_postal_address;
  char *department;
  char *display_name_prefix;
  char *first_name;
  char *followup;
  char *free_busy_address;
  char *ftp_site;
  char *fullname;
  int32_t  gender;
  char *gov_id;
  char *hobbies;
  char *home_address;
  char *home_city;
  char *home_country;
  char *home_fax;
  char *home_phone;
  char *home_phone2;
  char *home_po_box;
  char *home_postal_code;
  char *home_state;
  char *home_street;
  char *initials;
  char *isdn_phone;
  char *job_title;
  char *keyword;
  char *language;
  char *location;
  int32_t  mail_permission;
  char *manager_name;
  char *middle_name;
  char *mileage;
  char *mobile_phone;
  char *nickname;
  char *office_loc;
  char *org_id;
  char *other_address;
  char *other_city;
  char *other_country;
  char *other_phone;
  char *other_po_box;
  char *other_postal_code;
  char *other_state;
  char *other_street;
  char *pager_phone;
  char *personal_homepage;
  char *pref_name;
  char *primary_fax;
  char *primary_phone;
  char *profession;
  char *radio_phone;
  int32_t  rich_text;
  char *spouse_name;
  char *suffix;
  char *surname;
  char *telex;
  char *transmittable_display_name;
  char *ttytdd_phone;
  FILETIME *wedding_anniversary;
} pst_item_contact;

typedef struct _pst_item_attach {
  char *filename1;
  char *filename2;
  char *mimetype;
  char *data;
  size_t   size;
  int32_t  id2_val;
  int32_t  id_val; // calculated from id2_val during creation of record
  int32_t  method;
  int32_t  position;
  int32_t  sequence;
  struct _pst_item_attach *next;
} pst_item_attach;

typedef struct _pst_item_extra_field {
  char *field_name;
  char *value;
  struct _pst_item_extra_field *next;
} pst_item_extra_field;

typedef struct _pst_item_journal {
  FILETIME *end;
  FILETIME *start;
  char *type;
} pst_item_journal;

typedef struct _pst_item_appointment {
  FILETIME *end;
  char *location;
  FILETIME *reminder;
  FILETIME *start;
  char *timezonestring;
  int32_t showas;
  int32_t label;
  int32_t all_day;
} pst_item_appointment;

typedef struct _pst_item {
  struct _pst_item_email *email; // data reffering to email
  struct _pst_item_folder *folder; // data reffering to folder
  struct _pst_item_contact *contact; // data reffering to contact
  struct _pst_item_attach *attach; // linked list of attachments
  struct _pst_item_message_store * message_store; // data referring to the message store
  struct _pst_item_extra_field *extra_fields; // linked list of extra headers and such
  struct _pst_item_journal *journal; // data reffering to a journal entry
  struct _pst_item_appointment *appointment; // data reffering to a calendar entry
  int32_t type;
  char *ascii_type;
  char *file_as;
  char *comment;
  int32_t  message_size;
  char *outlook_version;
  char *record_key; // probably 16 bytes long.
  size_t record_key_size;
  int32_t  response_requested;
  FILETIME *create_date;
  FILETIME *modify_date;
  int32_t private_member;
} pst_item;

typedef struct _pst_x_attrib_ll {
  int32_t type;
  int32_t mytype;
  int32_t map;
  void *data;
  struct _pst_x_attrib_ll *next;
} pst_x_attrib_ll;

typedef struct _pst_file {
  pst_index_ll *i_head, *i_tail;
  pst_index2_ll *i2_head;
  pst_desc_ll *d_head, *d_tail;
  pst_x_attrib_ll *x_head;
  int32_t index1;
  int32_t index1_count;
  int32_t index2;
  int32_t index2_count;
  FILE * fp;				// file pointer to opened PST file
  size_t size;				// pst file size
  unsigned char encryption; // pst encryption setting
  unsigned char ind_type;	// pst index type
} pst_file;

typedef struct _pst_block_offset {
  int16_t from;
  int16_t to;
} pst_block_offset;

typedef struct _pst_block_offset_pointer {
  unsigned char *from;
  unsigned char *to;
  int			needfree;
} pst_block_offset_pointer;

struct _pst_num_item {
  u_int32_t id;
  unsigned char *data;
  int32_t type;
  size_t size;
  char *extra;
};

typedef struct _pst_num_array {
  int32_t count_item;
  int32_t count_array;
  struct _pst_num_item ** items;
  struct _pst_num_array *next;
} pst_num_array;

struct holder {
  unsigned char **buf;
  FILE * fp;
  int32_t base64;
  char base64_extra_chars[3];
  int32_t base64_extra;
};

// prototypes
int32_t pst_open(pst_file *pf, char *name, char *mode);
int32_t pst_close(pst_file *pf);
pst_desc_ll * pst_getTopOfFolders(pst_file *pf, pst_item *root);
int32_t pst_attach_to_mem(pst_file *pf, pst_item_attach *attach, unsigned char **b);
int32_t pst_attach_to_file(pst_file *pf, pst_item_attach *attach, FILE* fp);
int32_t pst_attach_to_file_base64(pst_file *pf, pst_item_attach *attach, FILE* fp);
int32_t pst_load_index (pst_file *pf);
pst_desc_ll* pst_getNextDptr(pst_desc_ll* d);
int32_t pst_load_extended_attributes(pst_file *pf);

int32_t _pst_build_id_ptr(pst_file *pf, int32_t offset, int32_t depth, int32_t linku1, u_int32_t start_val, u_int32_t end_val);
int32_t _pst_build_desc_ptr (pst_file *pf, int32_t offset, int32_t depth, int32_t linku1, u_int32_t *high_id, u_int32_t start_id, u_int32_t end_val);
pst_item* _pst_getItem(pst_file *pf, pst_desc_ll *d_ptr);
void * _pst_parse_item (pst_file *pf, pst_desc_ll *d_ptr);
pst_num_array * _pst_parse_block(pst_file *pf, u_int32_t block_id, pst_index2_ll *i2_head);
int32_t _pst_process(pst_num_array *list, pst_item *item);
int32_t _pst_free_list(pst_num_array *list);
void _pst_freeItem(pst_item *item);
int32_t _pst_free_id2(pst_index2_ll * head);
int32_t _pst_free_id (pst_index_ll *head);
int32_t _pst_free_desc (pst_desc_ll *head);
int32_t _pst_free_xattrib(pst_x_attrib_ll *x);
int32_t _pst_getBlockOffsetPointer(pst_file *pf, pst_index2_ll *i2_head, unsigned char *buf, int32_t read_size, int32_t i_offset, int32_t offset, pst_block_offset_pointer *p);
int32_t _pst_getBlockOffset(unsigned char *buf, int32_t read_size, int32_t i_offset, int32_t offset, pst_block_offset *p);
pst_index2_ll * _pst_build_id2(pst_file *pf, pst_index_ll* list, pst_index2_ll* head_ptr);
pst_index_ll * _pst_getID(pst_file* pf, u_int32_t id);
pst_index_ll * _pst_getID2(pst_index2_ll * ptr, u_int32_t id);
pst_desc_ll * _pst_getDptr(pst_file *pf, u_int32_t id);
size_t _pst_read_block_size(pst_file *pf, int32_t offset, size_t size, char ** buf, int32_t do_enc,
			 unsigned char is_index);
int32_t _pst_decrypt(unsigned char *buf, size_t size, int32_t type);
int32_t _pst_getAtPos(FILE *fp, int32_t pos, void* buf, u_int32_t size);
int32_t _pst_get (FILE *fp, void *buf, u_int32_t size);
size_t _pst_ff_getIDblock_dec(pst_file *pf, u_int32_t id, unsigned char **b);
size_t _pst_ff_getIDblock(pst_file *pf, u_int32_t id, unsigned char** b);
size_t _pst_ff_getID2block(pst_file *pf, u_int32_t id2, pst_index2_ll *id2_head, unsigned char** buf);
size_t _pst_ff_getID2data(pst_file *pf, pst_index_ll *ptr, struct holder *h);
size_t _pst_ff_compile_ID(pst_file *pf, u_int32_t id, struct holder *h, int32_t size);

int32_t pst_strincmp(char *a, char *b, int32_t x);
int32_t pst_stricmp(char *a, char *b);
size_t pst_fwrite(const void*ptr, size_t size, size_t nmemb, FILE*stream);
char * _pst_wide_to_single(char *wt, int32_t size);
// DEBUG functions
int32_t _pst_printDptr(pst_file *pf);
int32_t _pst_printIDptr(pst_file* pf);
int32_t _pst_printID2ptr(pst_index2_ll *ptr);
void * xmalloc(size_t size);

#endif // defined LIBPST_H
