/***
 * libpst.h
 * Part of LibPST project
 * Written by David Smith
 *            dave.s@earthcorp.com
 */
// LibPST - Library for Accessing Outlook .pst files
// Dave Smith - davesmith@users.sourceforge.net

#ifndef __PST_LIBPST_H
#define __PST_LIBPST_H

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
#define PST_TYPE_SCHEDULE    2
#define PST_TYPE_APPOINTMENT 8
#define PST_TYPE_CONTACT     9
#define PST_TYPE_JOURNAL    10
#define PST_TYPE_STICKYNOTE 11
#define PST_TYPE_TASK       12
#define PST_TYPE_OTHER      13
#define PST_TYPE_REPORT     14
#define PST_TYPE_MAX        15

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
#define PST_FREEBUSY_FREE          0
#define PST_FREEBUSY_TENTATIVE     1
#define PST_FREEBUSY_BUSY          2
#define PST_FREEBUSY_OUT_OF_OFFICE 3

// defines labels for appointment->label
#define PST_APP_LABEL_NONE        0
#define PST_APP_LABEL_IMPORTANT   1
#define PST_APP_LABEL_BUSINESS    2
#define PST_APP_LABEL_PERSONAL    3
#define PST_APP_LABEL_VACATION    4
#define PST_APP_LABEL_MUST_ATTEND 5
#define PST_APP_LABEL_TRAVEL_REQ  6
#define PST_APP_LABEL_NEEDS_PREP  7
#define PST_APP_LABEL_BIRTHDAY    8
#define PST_APP_LABEL_ANNIVERSARY 9
#define PST_APP_LABEL_PHONE_CALL  10

// define type of recuring event
#define PST_APP_RECUR_NONE        0
#define PST_APP_RECUR_DAILY       1
#define PST_APP_RECUR_WEEKLY      2
#define PST_APP_RECUR_MONTHLY     3
#define PST_APP_RECUR_YEARLY      4

// define attachment types
#define PST_ATTACH_NONE             0
#define PST_ATTACH_BY_VALUE         1
#define PST_ATTACH_BY_REF           2
#define PST_ATTACH_BY_REF_RESOLV    3
#define PST_ATTACH_BY_REF_ONLY      4
#define PST_ATTACH_EMBEDDED         5
#define PST_ATTACH_OLE              6

// define flags
#define PST_FLAG_READ			0x01
#define PST_FLAG_UNMODIFIED		0x02
#define PST_FLAG_SUBMIT			0x04
#define PST_FLAG_UNSENT			0x08
#define PST_FLAG_HAS_ATTACHMENT	0x10
#define PST_FLAG_FROM_ME		0x20
#define PST_FLAG_ASSOCIATED		0x40
#define PST_FLAG_RESEND			0x80
#define PST_FLAG_RN_PENDING		0x100
#define PST_FLAG_NRN_PENDING	0x200


typedef struct pst_entryid {
    int32_t u1;
    char entryid[16];
    uint32_t id;
} pst_entryid;


typedef struct pst_index_ll {
    uint64_t i_id;
    uint64_t offset;
    uint64_t size;
    uint64_t inflated_size;
    int64_t  u1;
} pst_index_ll;


typedef struct pst_id2_tree {
    uint64_t            id2;
    pst_index_ll        *id;
    struct pst_id2_tree *child;
    struct pst_id2_tree *next;
} pst_id2_tree;


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
} pst_desc_tree;


/** The string is either utf8 encoded, or it is in the code page
 *  specified by the containing mapi object. It can be forced into
 *  utf8 by calling pst_convert_utf8() or pst_convert_utf8_null().
 */
typedef struct pst_string {
    /** @li 1 true
     *  @li 0 false */
    int     is_utf8;
    char   *str;
} pst_string;


/** a simple wrapper for binary blobs */
typedef struct pst_binary {
    size_t  size;
    char   *data;
} pst_binary;


/** This contains the email related mapi elements
 */
typedef struct pst_item_email {
    /** mapi element 0x0e06 PR_MESSAGE_DELIVERY_TIME */
    FILETIME   *arrival_date;
    /** mapi element 0x0002 PR_ALTERNATE_RECIPIENT_ALLOWED
     *  @li 1 true
     *  @li 0 not set
     *  @li -1 false */
    int         autoforward;
    /** mapi element 0x0e03 PR_DISPLAY_CC */
    pst_string  cc_address;
    /** mapi element 0x0e02 PR_DISPLAY_BCC */
    pst_string  bcc_address;
    /** mapi element 0x0071 PR_CONVERSATION_INDEX */
    pst_binary  conversation_index;
    /** mapi element 0x3a03 PR_CONVERSION_PROHIBITED
     *  @li 1 true
     *  @li 0 false */
    int         conversion_prohibited;
    /** mapi element 0x0e01 PR_DELETE_AFTER_SUBMIT
     *  @li 1 true
     *  @li 0 false */
    int         delete_after_submit;
    /** mapi element 0x0023 PR_ORIGINATOR_DELIVERY_REPORT_REQUESTED
     *  @li 1 true
     *  @li 0 false */
    int         delivery_report;
    /** mapi element 0x6f04 */
    pst_binary  encrypted_body;
    /** mapi element 0x6f02 */
    pst_binary  encrypted_htmlbody;
    /** mapi element 0x007d PR_TRANSPORT_MESSAGE_HEADERS */
    pst_string  header;
    /** mapi element 0x1013 */
    pst_string  htmlbody;
    /** mapi element 0x0017 PR_IMPORTANCE
     *  @li 0 low
     *  @li 1 normal
     *  @li 2 high */
    int32_t     importance;
    /** mapi element 0x1042 */
    pst_string  in_reply_to;
    /** mapi element 0x0058 PR_MESSAGE_CC_ME, this user is listed explicitly in the CC address
     *  @li 1 true
     *  @li 0 false */
    int         message_cc_me;
    /** mapi element 0x0059 PR_MESSAGE_RECIP_ME, this user appears in TO, CC or BCC address list
     *  @li 1 true
     *  @li 0 false */
    int         message_recip_me;
    /** mapi element 0x0057 PR_MESSAGE_TO_ME, this user is listed explicitly in the TO address
     *  @li 1 true
     *  @li 0 false */
    int         message_to_me;
    /** mapi element 0x1035 */
    pst_string  messageid;
    /** mapi element 0x002e PR_ORIGINAL_SENSITIVITY
     *  @li 0=none
     *  @li 1=personal
     *  @li 2=private
     *  @li 3=company confidential */
    int32_t     original_sensitivity;
    /** mapi element 0x0072 PR_ORIGINAL_DISPLAY_BCC */
    pst_string  original_bcc;
    /** mapi element 0x0073 PR_ORIGINAL_DISPLAY_CC */
    pst_string  original_cc;
    /** mapi element 0x0074 PR_ORIGINAL_DISPLAY_TO */
    pst_string  original_to;
    /** mapi element 0x0051 PR_RECEIVED_BY_SEARCH_KEY */
    pst_string  outlook_recipient;
    /** mapi element 0x0044 PR_RCVD_REPRESENTING_NAME */
    pst_string  outlook_recipient_name;
    /** mapi element 0x0052 PR_RCVD_REPRESENTING_SEARCH_KEY */
    pst_string  outlook_recipient2;
    /** mapi element 0x003b PR_SENT_REPRESENTING_SEARCH_KEY */
    pst_string  outlook_sender;
    /** mapi element 0x0042 PR_SENT_REPRESENTING_NAME */
    pst_string  outlook_sender_name;
    /** mapi element 0x0c1d PR_SENDER_SEARCH_KEY */
    pst_string  outlook_sender2;
    /** mapi element 0x0026 PR_PRIORITY
     *  @li 0 nonurgent
     *  @li 1 normal
     *  @li 2 urgent */
    /** mapi element  */
    int32_t     priority;
    /** mapi element 0x0070 PR_CONVERSATION_TOPIC */
    pst_string  processed_subject;
    /** mapi element 0x0029 PR_READ_RECEIPT_REQUESTED
     *  @li 1 true
     *  @li 0 false */
    int         read_receipt;
    /** mapi element 0x0075 PR_RECEIVED_BY_ADDRTYPE */
    pst_string  recip_access;
    /** mapi element 0x0076 PR_RECEIVED_BY_EMAIL_ADDRESS */
    pst_string  recip_address;
    /** mapi element 0x0077 PR_RCVD_REPRESENTING_ADDRTYPE */
    pst_string  recip2_access;
    /** mapi element 0x0078 PR_RCVD_REPRESENTING_EMAIL_ADDRESS */
    pst_string  recip2_address;
    /** mapi element 0x0c17 PR_REPLY_REQUESTED
     *  @li 1 true
     *  @li 0 false */
    int         reply_requested;
    /** mapi element 0x0050 PR_REPLY_RECIPIENT_NAMES */
    pst_string  reply_to;
    /** mapi element 0x1046, this seems to be the message-id of the rfc822 mail that is being returned */
    pst_string  return_path_address;
    /** mapi element 0x1007 PR_RTF_SYNC_BODY_COUNT,
     *  a count of the *significant* charcters in the rtf body. Doesn't count
     *  whitespace and other ignorable characters. */
    int32_t     rtf_body_char_count;
    /** mapi element 0x1006 PR_RTF_SYNC_BODY_CRC */
    int32_t     rtf_body_crc;
    /** mapi element 0x1008 PR_RTF_SYNC_BODY_TAG,
     *  the first couple of lines of RTF body so that after modification, then beginning can
     *  once again be found. */
    pst_string  rtf_body_tag;
    /** mapi element 0x1009 PR_RTF_COMPRESSED,
     *  the compressed rtf body data.
     *  Use pst_lzfu_decompress() to retrieve the actual rtf body data. */
    pst_binary  rtf_compressed;
    /** mapi element 0x0e1f PR_RTF_IN_SYNC,
     *  True means that the rtf version is same as text body.
     *  False means rtf version is more up-to-date than text body.
     *  If this value doesn't exist, text body is more up-to-date than rtf and
     *  cannot update to the rtf.
     *  @li 1 true
     *  @li 0 false */
    int         rtf_in_sync;
    /** mapi element 0x1010 PR_RTF_SYNC_PREFIX_COUNT,
     *  a count of the ignored characters before the first significant character */
    int32_t     rtf_ws_prefix_count;
    /** mapi element 0x1011 PR_RTF_SYNC_TRAILING_COUNT,
     *  a count of the ignored characters after the last significant character */
    int32_t     rtf_ws_trailing_count;
    /** mapi element 0x0064 PR_SENT_REPRESENTING_ADDRTYPE */
    pst_string  sender_access;
    /** mapi element 0x0065 PR_SENT_REPRESENTING_EMAIL_ADDRESS */
    pst_string  sender_address;
    /** mapi element 0x0c1e PR_SENDER_ADDRTYPE */
    pst_string  sender2_access;
    /** mapi element 0x0c1f PR_SENDER_EMAIL_ADDRESS */
    pst_string  sender2_address;
    /** mapi element 0x0036 PR_SENSITIVITY
     *  @li 0=none
     *  @li 1=personal
     *  @li 2=private
     *  @li 3=company confidential */
    int32_t     sensitivity;
    /** mapi element 0x0039 PR_CLIENT_SUBMIT_TIME */
    FILETIME    *sent_date;
    /** mapi element 0x0e0a PR_SENTMAIL_ENTRYID */
    pst_entryid *sentmail_folder;
    /** mapi element 0x0e04 PR_DISPLAY_TO */
    pst_string  sentto_address;
    /** mapi element 0x1001 PR_REPORT_TEXT, delivery report dsn body */
    pst_string  report_text;
    /** mapi element 0x0032 PR_REPORT_TIME, delivery report time */
    FILETIME   *report_time;
    /** mapi element 0x0c04 PR_NDR_REASON_CODE */
    int32_t     ndr_reason_code;
    /** mapi element 0x0c05 PR_NDR_DIAG_CODE */
    int32_t     ndr_diag_code;
    /** mapi element 0x0c1b PR_SUPPLEMENTARY_INFO */
    pst_string  supplementary_info;
    /** mapi element 0x0c20 PR_NDR_STATUS_CODE */
    int32_t     ndr_status_code;

    // elements added for .msg processing
    /** mapi element 0x0040 PR_RECEIVED_BY_NAME */
    pst_string  outlook_received_name1;
    /** mapi element 0x0c1a PR_SENDER_NAME */
    pst_string  outlook_sender_name2;
    /** mapi element 0x0e1d PR_NORMALIZED_SUBJECT */
    pst_string  outlook_normalized_subject;
    /** mapi element 0x300b PR_SEARCH_KEY */
    pst_string  outlook_search_key;
} pst_item_email;


/** This contains the folder related mapi elements
 */
typedef struct pst_item_folder {
    /** mapi element 0x3602 PR_CONTENT_COUNT */
    int32_t  item_count;
    /** mapi element 0x3603 PR_CONTENT_UNREAD */
    int32_t  unseen_item_count;
    /** mapi element 0x3617 PR_ASSOC_CONTENT_COUNT
        Associated content are items that are attached to this folder, but are hidden from users.
    */
    int32_t  assoc_count;
    /** mapi element 0x360a PR_SUBFOLDERS
     *  @li 1 true
     *  @li 0 false */
    /** mapi element  */
    int      subfolder;
} pst_item_folder;


/** This contains the message store related mapi elements
 */
typedef struct pst_item_message_store {
    /** mapi element 0x35e0 */
    pst_entryid *top_of_personal_folder;
    /** mapi element 0x35e2 */
    pst_entryid *default_outbox_folder;
    /** mapi element 0x35e3 */
    pst_entryid *deleted_items_folder;
    /** mapi element 0x35e4 */
    pst_entryid *sent_items_folder;
    /** mapi element 0x35e5 */
    pst_entryid *user_views_folder;
    /** mapi element 0x35e6 */
    pst_entryid *common_view_folder;
    /** mapi element 0x35e7 */
    pst_entryid *search_root_folder;
    /** mapi element 0x7c07 */
    pst_entryid *top_of_folder;
    /** mapi element 0x35df,
     *  bit mask of folders in this message store
     *  @li  0x1 FOLDER_IPM_SUBTREE_VALID
     *  @li  0x2 FOLDER_IPM_INBOX_VALID
     *  @li  0x4 FOLDER_IPM_OUTBOX_VALID
     *  @li  0x8 FOLDER_IPM_WASTEBOX_VALID
     *  @li 0x10 FOLDER_IPM_SENTMAIL_VALID
     *  @li 0x20 FOLDER_VIEWS_VALID
     *  @li 0x40 FOLDER_COMMON_VIEWS_VALID
     *  @li 0x80 FOLDER_FINDER_VALID */
    int32_t valid_mask;
    /** mapi element 0x76ff */
    int32_t pwd_chksum;
} pst_item_message_store;


/** This contains the contact related mapi elements
 */
typedef struct pst_item_contact {
    /** mapi element 0x3a00 PR_ACCOUNT */
    pst_string  account_name;
    /** mapi element 0x3003 PR_EMAIL_ADDRESS, or 0x8083 */
    pst_string  address1;
    /** mapi element 0x8085 */
    pst_string  address1a;
    /** mapi element 0x8084 */
    pst_string  address1_desc;
    /** mapi element 0x3002 PR_ADDRTYPE, or 0x8082 */
    pst_string  address1_transport;
    /** mapi element 0x8093 */
    pst_string  address2;
    /** mapi element 0x8095 */
    pst_string  address2a;
    /** mapi element 0x8094 */
    pst_string  address2_desc;
    /** mapi element 0x8092 */
    pst_string  address2_transport;
    /** mapi element 0x80a3 */
    pst_string  address3;
    /** mapi element 0x80a5 */
    pst_string  address3a;
    /** mapi element 0x80a4 */
    pst_string  address3_desc;
    /** mapi element 0x80a2 */
    pst_string  address3_transport;
    /** mapi element 0x3a30 PR_ASSISTANT */
    pst_string  assistant_name;
    /** mapi element 0x3a2e PR_ASSISTANT_TELEPHONE_NUMBER */
    pst_string  assistant_phone;
    /** mapi element 0x8535 */
    pst_string  billing_information;
    /** mapi element 0x3a42 PR_BIRTHDAY */
    FILETIME   *birthday;
    /** mapi element 0x801b */
    pst_string  business_address;
    /** mapi element 0x3a27 PR_BUSINESS_ADDRESS_CITY */
    pst_string  business_city;
    /** mapi element 0x3a26 PR_BUSINESS_ADDRESS_COUNTRY */
    pst_string  business_country;
    /** mapi element 0x3a24 PR_BUSINESS_FAX_NUMBER */
    pst_string  business_fax;
    /** mapi element 0x3a51 PR_BUSINESS_HOME_PAGE */
    pst_string  business_homepage;
    /** mapi element 0x3a08 PR_BUSINESS_TELEPHONE_NUMBER */
    pst_string  business_phone;
    /** mapi element 0x3a1b PR_BUSINESS2_TELEPHONE_NUMBER */
    pst_string  business_phone2;
    /** mapi element 0x3a2b PR_BUSINESS_PO_BOX */
    pst_string  business_po_box;
    /** mapi element 0x3a2a PR_BUSINESS_POSTAL_CODE */
    pst_string  business_postal_code;
    /** mapi element 0x3a28 PR_BUSINESS_ADDRESS_STATE_OR_PROVINCE */
    pst_string  business_state;
    /** mapi element 0x3a29 PR_BUSINESS_ADDRESS_STREET */
    pst_string  business_street;
    /** mapi element 0x3a02 PR_CALLBACK_TELEPHONE_NUMBER */
    pst_string  callback_phone;
    /** mapi element 0x3a1e PR_CAR_TELEPHONE_NUMBER */
    pst_string  car_phone;
    /** mapi element 0x3a57 PR_COMPANY_MAIN_PHONE_NUMBER */
    pst_string  company_main_phone;
    /** mapi element 0x3a16 PR_COMPANY_NAME */
    pst_string  company_name;
    /** mapi element 0x3a49 PR_COMPUTER_NETWORK_NAME */
    pst_string  computer_name;
    /** mapi element 0x3a4a PR_CUSTOMER_ID */
    pst_string  customer_id;
    /** mapi element 0x3a15 PR_POSTAL_ADDRESS */
    pst_string  def_postal_address;
    /** mapi element 0x3a18 PR_DEPARTMENT_NAME */
    pst_string  department;
    /** mapi element 0x3a45 PR_DISPLAY_NAME_PREFIX */
    pst_string  display_name_prefix;
    /** mapi element 0x3a06 PR_GIVEN_NAME */
    pst_string  first_name;
    /** mapi element 0x8530 */
    pst_string  followup;
    /** mapi element 0x80d8 */
    pst_string  free_busy_address;
    /** mapi element 0x3a4c PR_FTP_SITE */
    pst_string  ftp_site;
    /** mapi element 0x8005 */
    pst_string  fullname;
    /** mapi element 0x3a4d PR_GENDER
     *  @li 0 unspecified
     *  @li 1 female
     *  @li 2 male */
    int16_t     gender;
    /** mapi element 0x3a07 PR_GOVERNMENT_ID_NUMBER */
    pst_string  gov_id;
    /** mapi element 0x3a43 PR_HOBBIES */
    pst_string  hobbies;
    /** mapi element 0x801a */
    pst_string  home_address;
    /** mapi element 0x3a59 PR_HOME_ADDRESS_CITY */
    pst_string  home_city;
    /** mapi element 0x3a5a PR_HOME_ADDRESS_COUNTRY */
    pst_string  home_country;
    /** mapi element 0x3a25 PR_HOME_FAX_NUMBER */
    pst_string  home_fax;
    /** mapi element 0x3a09 PR_HOME_TELEPHONE_NUMBER */
    pst_string  home_phone;
    /** mapi element 0x3a2f PR_HOME2_TELEPHONE_NUMBER */
    pst_string  home_phone2;
    /** mapi element 0x3a5e PR_HOME_ADDRESS_POST_OFFICE_BOX */
    pst_string  home_po_box;
    /** mapi element 0x3a5b PR_HOME_ADDRESS_POSTAL_CODE */
    pst_string  home_postal_code;
    /** mapi element 0x3a5c PR_HOME_ADDRESS_STATE_OR_PROVINCE */
    pst_string  home_state;
    /** mapi element 0x3a5d PR_HOME_ADDRESS_STREET */
    pst_string  home_street;
    /** mapi element 0x3a0a PR_INITIALS */
    pst_string  initials;
    /** mapi element 0x3a2d PR_ISDN_NUMBER */
    pst_string  isdn_phone;
    /** mapi element 0x3a17 PR_TITLE */
    pst_string  job_title;
    /** mapi element 0x3a0b PR_KEYWORD */
    pst_string  keyword;
    /** mapi element 0x3a0c PR_LANGUAGE */
    pst_string  language;
    /** mapi element 0x3a0d PR_LOCATION */
    pst_string  location;
    /** mapi element 0x3a0e PR_MAIL_PERMISSION
     *  @li 1 true
     *  @li 0 false */
    int         mail_permission;
    /** mapi element 0x3a4e PR_MANAGER_NAME */
    pst_string  manager_name;
    /** mapi element 0x3a44 PR_MIDDLE_NAME */
    pst_string  middle_name;
    /** mapi element 0x8534 */
    pst_string  mileage;
    /** mapi element 0x3a1c PR_MOBILE_TELEPHONE_NUMBER */
    pst_string  mobile_phone;
    /** mapi element 0x3a4f PR_NICKNAME */
    pst_string  nickname;
    /** mapi element 0x3a19 PR_OFFICE_LOCATION */
    pst_string  office_loc;
    /** mapi element 0x3a0f PR_MHS_COMMON_NAME */
    pst_string  common_name;
    /** mapi element 0x3a10 PR_ORGANIZATIONAL_ID_NUMBER */
    pst_string  org_id;
    /** mapi element 0x801c */
    pst_string  other_address;
    /** mapi element 0x3a5f PR_OTHER_ADDRESS_CITY */
    pst_string  other_city;
    /** mapi element 0x3a60 PR_OTHER_ADDRESS_COUNTRY */
    pst_string  other_country;
    /** mapi element 0x3a1f PR_OTHER_TELEPHONE_NUMBER */
    pst_string  other_phone;
    /** mapi element 0x3a64 PR_OTHER_ADDRESS_POST_OFFICE_BOX */
    pst_string  other_po_box;
    /** mapi element 0x3a61 PR_OTHER_ADDRESS_POSTAL_CODE */
    pst_string  other_postal_code;
    /** mapi element 0x3a62 PR_OTHER_ADDRESS_STATE_OR_PROVINCE */
    pst_string  other_state;
    /** mapi element 0x3a63 PR_OTHER_ADDRESS_STREET */
    pst_string  other_street;
    /** mapi element 0x3a21 PR_PAGER_TELEPHONE_NUMBER */
    pst_string  pager_phone;
    /** mapi element 0x3a50 PR_PERSONAL_HOME_PAGE */
    pst_string  personal_homepage;
    /** mapi element 0x3a47 PR_PREFERRED_BY_NAME */
    pst_string  pref_name;
    /** mapi element 0x3a23 PR_PRIMARY_FAX_NUMBER */
    pst_string  primary_fax;
    /** mapi element 0x3a1a PR_PRIMARY_TELEPHONE_NUMBER */
    pst_string  primary_phone;
    /** mapi element 0x3a46 PR_PROFESSION */
    pst_string  profession;
    /** mapi element 0x3a1d PR_RADIO_TELEPHONE_NUMBER */
    pst_string  radio_phone;
    /** mapi element 0x3a40 PR_SEND_RICH_INFO
     *  @li 1 true
     *  @li 0 false */
    int         rich_text;
    /** mapi element 0x3a48 PR_SPOUSE_NAME */
    pst_string  spouse_name;
    /** mapi element 0x3a05 PR_GENERATION (Jr., Sr., III, etc) */
    pst_string  suffix;
    /** mapi element 0x3a11 PR_SURNAME */
    pst_string  surname;
    /** mapi element 0x3a2c PR_TELEX_NUMBER */
    pst_string  telex;
    /** mapi element 0x3a20 PR_TRANSMITTABLE_DISPLAY_NAME */
    pst_string  transmittable_display_name;
    /** mapi element 0x3a4b PR_TTYTDD_PHONE_NUMBER */
    pst_string  ttytdd_phone;
    /** mapi element 0x3a41 PR_WEDDING_ANNIVERSARY */
    FILETIME   *wedding_anniversary;
    /** mapi element 0x8045 */
    pst_string  work_address_street;
    /** mapi element 0x8046 */
    pst_string  work_address_city;
    /** mapi element 0x8047 */
    pst_string  work_address_state;
    /** mapi element 0x8048 */
    pst_string  work_address_postalcode;
    /** mapi element 0x8049 */
    pst_string  work_address_country;
    /** mapi element 0x804a */
    pst_string  work_address_postofficebox;
} pst_item_contact;


/** This contains the attachment related mapi elements
 */
typedef struct pst_item_attach {
    /** mapi element 0x3704 PR_ATTACH_FILENAME */
    pst_string      filename1;
    /** mapi element 0x3707 PR_ATTACH_LONG_FILENAME */
    pst_string      filename2;
    /** mapi element 0x370e PR_ATTACH_MIME_TAG */
    pst_string      mimetype;
    /** mapi element 0x3712 PR_ATTACH_CONTENT_ID  */
    pst_string      content_id;
    /** mapi element 0x3701 PR_ATTACH_DATA_OBJ */
    pst_binary      data;
    /** only used if the attachment is by reference, in which case this is the id2 reference */
    uint64_t        id2_val;
    /** calculated from id2_val during creation of record */
    uint64_t        i_id;
    /** id2 tree needed to resolve attachments by reference */
    pst_id2_tree    *id2_head;
    /** mapi element 0x3705 PR_ATTACH_METHOD
     *  @li 0 no attachment
     *  @li 1 attach by value
     *  @li 2 attach by reference
     *  @li 3 attach by reference resolve
     *  @li 4 attach by reference only
     *  @li 5 embedded message
     *  @li 6 OLE */
    int32_t         method;
    /** mapi element 0x370b PR_RENDERING_POSITION */
    int32_t         position;
    /** mapi element 0x3710 PR_ATTACH_MIME_SEQUENCE */
    int32_t         sequence;
    struct pst_item_attach *next;
} pst_item_attach;


/** linked list of extra header fields */
typedef struct pst_item_extra_field {
    char   *field_name;
    char   *value;
    struct pst_item_extra_field *next;
} pst_item_extra_field;


/** This contains the journal related mapi elements
 */
typedef struct pst_item_journal {
    /** mapi element 0x8706 */
    FILETIME   *start;
    /** mapi element 0x8708 */
    FILETIME   *end;
    /** mapi element 0x8700 */
    pst_string  type;
    /** mapi element 0x8712 */
    pst_string  description;
} pst_item_journal;


/** This contains the recurrence data separated into fields.
    http://www.geocities.com/cainrandom/dev/MAPIRecurrence.html
*/
typedef struct pst_recurrence {
    /** 0x30043004 */
    uint32_t    signature;
    /** @li 0 daily
     *  @li 1 weekly
     *  @li 2 monthly
     *  @li 3 yearly */
    uint32_t    type;
    /** implies number of recurrence parameters
     *  @li 0 has 3 parameters
     *  @li 1 has 4 parameters
     *  @li 2 has 4 parameters
     *  @li 3 has 5 parameters
     */
    uint32_t    sub_type;
    /** must be contiguous, not an array to make python interface easier */
    uint32_t    parm1;
    uint32_t    parm2;
    uint32_t    parm3;
    uint32_t    parm4;
    uint32_t    parm5;
    /** type of termination of the recurrence
        @li 0 terminates on a date
        @li 1 terminates based on integer number of occurrences
        @li 2 never terminates
     */
    uint32_t    termination;
    /** recurrence interval in terms of the recurrence type */
    uint32_t    interval;
    /** bit mask of days of the week */
    uint32_t    bydaymask;
    /** day of month for monthly and yearly recurrences */
    uint32_t    dayofmonth;
    /** month of year for yearly recurrences */
    uint32_t    monthofyear;
    /** occurence of day for 2nd Tuesday of month, in which case position is 2 */
    uint32_t    position;
    /** number of occurrences, even if recurrence terminates based on date */
    uint32_t    count;
    // there is more data, including the termination date,
    // but we can get that from other mapi elements.
} pst_recurrence;


/** This contains the appointment related mapi elements
 */
typedef struct pst_item_appointment {
    /** mapi element 0x820d PR_OUTLOOK_EVENT_START_DATE */
    FILETIME   *start;
    /** mapi element 0x820e PR_OUTLOOK_EVENT_START_END */
    FILETIME   *end;
    /** mapi element 0x8208 PR_OUTLOOK_EVENT_LOCATION */
    pst_string  location;
    /** mapi element 0x8503 PR_OUTLOOK_COMMON_REMINDER_SET
     *  @li 1 true
     *  @li 0 false */
    int         alarm;
    /** mapi element 0x8560 */
    FILETIME   *reminder;
    /** mapi element 0x8501 PR_OUTLOOK_COMMON_REMINDER_MINUTES_BEFORE */
    int32_t     alarm_minutes;
    /** mapi element 0x851f */
    pst_string  alarm_filename;
    /** mapi element 0x8234 */
    pst_string  timezonestring;
    /** mapi element 0x8205 PR_OUTLOOK_EVENT_SHOW_TIME_AS
     *  @li 0 free
     *  @li 1 tentative
     *  @li 2 busy
     *  @li 3 out of office*/
    int32_t     showas;
    /** mapi element 0x8214
     *  @li 0 None
     *  @li 1 Important
     *  @li 2 Business
     *  @li 3 Personal
     *  @li 4 Vacation
     *  @li 5 Must Attend
     *  @li 6 Travel Required
     *  @li 7 Needs Preparation
     *  @li 8 Birthday
     *  @li 9 Anniversary
     *  @li 10 Phone Call */
    int32_t     label;
    /** mapi element 0x8215 PR_OUTLOOK_EVENT_ALL_DAY
     *  @li 1 true
     *  @li 0 false */
    int         all_day;
    /** mapi element 0x8223 PR_OUTLOOK_EVENT_IS_RECURRING
     *  @li 1 true
     *  @li 0 false */
    int         is_recurring;
    /** mapi element 0x8231
     *  @li 0 none
     *  @li 1 daily
     *  @li 2 weekly
     *  @li 3 monthly
     *  @li 4 yearly */
    int32_t     recurrence_type;
    /** mapi element 0x8232 recurrence description */
    pst_string  recurrence_description;
    /** mapi element 0x8216 recurrence data */
    pst_binary  recurrence_data;
    /** mapi element 0x8235 PR_OUTLOOK_EVENT_RECURRENCE_START */
    FILETIME   *recurrence_start;
    /** mapi element 0x8236 PR_OUTLOOK_EVENT_RECURRENCE_END  */
    FILETIME   *recurrence_end;
} pst_item_appointment;


/** This contains the common mapi elements, and pointers to structures for
 *  each major mapi item type. It represents a complete mapi object.
 */
typedef struct pst_item {
    /** pointer to the pst_file */
    struct pst_file        *pf;
    /** block id that can be used to generate uid */
    uint64_t               block_id;
    /** email mapi elements */
    pst_item_email         *email;
    /** folder mapi elements */
    pst_item_folder        *folder;
    /** contact mapi elements */
    pst_item_contact       *contact;
    /** linked list of attachments */
    pst_item_attach        *attach;
    /** message store mapi elements */
    pst_item_message_store *message_store;
    /** linked list of extra headers and such */
    pst_item_extra_field   *extra_fields;
    /** journal mapi elements */
    pst_item_journal       *journal;
    /** calendar mapi elements */
    pst_item_appointment   *appointment;
    /** derived from mapi elements 0x001a PR_MESSAGE_CLASS or 0x3613 PR_CONTAINER_CLASS
     *  @li  1 PST_TYPE_NOTE
     *  @li  2 PST_TYPE_SCHEDULE
     *  @li  8 PST_TYPE_APPOINTMENT
     *  @li  9 PST_TYPE_CONTACT
     *  @li 10 PST_TYPE_JOURNAL
     *  @li 11 PST_TYPE_STICKYNOTE
     *  @li 12 PST_TYPE_TASK
     *  @li 13 PST_TYPE_OTHER
     *  @li 14 PST_TYPE_REPORT */
    int         type;
    /** mapi element 0x001a PR_MESSAGE_CLASS or 0x3613 PR_CONTAINER_CLASS */
    char       *ascii_type;
    /** mapi element 0x0e07 PR_MESSAGE_FLAGS
     *  @li 0x01 Read
     *  @li 0x02 Unmodified
     *  @li 0x04 Submit
     *  @li 0x08 Unsent
     *  @li 0x10 Has Attachments
     *  @li 0x20 From Me
     *  @li 0x40 Associated
     *  @li 0x80 Resend
     *  @li 0x100 RN Pending
     *  @li 0x200 NRN Pending */
    int32_t     flags;
    /** mapi element 0x3001 PR_DISPLAY_NAME */
    pst_string  file_as;
    /** mapi element 0x3004 PR_COMMENT */
    pst_string  comment;
    /** derived from extra_fields["content-type"] if it contains a charset= subfield  */
    pst_string  body_charset;
    /** mapi element 0x1000 PR_BODY */
    pst_string  body;
    /** mapi element 0x0037 PR_SUBJECT */
    pst_string  subject;
    /** mapi element 0x3fde PR_INTERNET_CPID */
    int32_t     internet_cpid;
    /** mapi element 0x3ffd PR_MESSAGE_CODEPAGE */
    int32_t     message_codepage;
    /** mapi element 0x0e08 PR_MESSAGE_SIZE */
    int32_t     message_size;
    /** mapi element 0x8554 PR_OUTLOOK_VERSION */
    pst_string  outlook_version;
    /** mapi element 0x0ff9 PR_RECORD_KEY */
    pst_binary  record_key;
    /** mapi element 0x65e3 PR_PREDECESSOR_CHANGE_LIST */
    pst_binary  predecessor_change;
    /** mapi element 0x0063 PR_RESPONSE_REQUESTED
     *  @li 1 true
     *  @li 0 false */
    int         response_requested;
    /** mapi element 0x3007 PR_CREATION_TIME */
    FILETIME   *create_date;
    /** mapi element 0x3008 PR_LAST_MODIFICATION_TIME */
    FILETIME   *modify_date;
    /** mapi element 0x002b PR_RECIPIENT_REASSIGNMENT_PROHIBITED
     *  @li 1 true
     *  @li 0 false */
    int         private_member;
} pst_item;


/** Linked list of extended attributes.
 *  This is used to convert mapi_id values in the pst file into
 *  cannonical mapi_id values to be used in this code. This list
 *  is kept in sorted order, where the key is the 'map' field.
 *  Some mapi_id values are converted to cannonical mapi_id values
 *  (PST_MAP_ATTRIB), and others are converted to a string
 *  (PST_ATTRIB_HEADER).
 */
typedef struct pst_x_attrib_ll {
    /** @li 1 PST_MAP_ATTRIB map->int attribute
        @li 2 PST_MAP_HEADER map->string header
     */
    uint32_t mytype;
    /** key for the mapping */
    uint32_t map;
    /** data target of the mapping, either uint32_t or string */
    void     *data;
    /** link to next item in the list */
    struct pst_x_attrib_ll *next;
} pst_x_attrib_ll;


/** this is only used for internal debugging */
typedef struct pst_block_recorder {
    struct pst_block_recorder  *next;
    int64_t                     offset;
    size_t                      size;
    int                         readcount;
} pst_block_recorder;


typedef struct pst_file {
    /** file pointer to opened PST file */
    FILE*   fp;
    /** original cwd when the file was opened */
    char*   cwd;
    /** original file name when the file was opened */
    char*   fname;
    /** default character set for items without one */
    const char*   charset;
    /** the array of index structures */
    pst_index_ll *i_table;
    size_t i_count, i_capacity;
    /** the head and tail of the top level of the descriptor tree */
    pst_desc_tree  *d_head, *d_tail;
    /** the head of the extended attributes linked list */
    pst_x_attrib_ll *x_head;
    /** the head of the block recorder, a debug artifact
     *  used to detect cases where we might read the same
     *  block multiple times while processing a pst file. */
    pst_block_recorder *block_head;

    /** @li 0 is 32-bit pst file, pre Outlook 2003;
     *  @li 1 is 64-bit pst file, Outlook 2003 or later;
     *  @li 2 is 64-bit OST file, Outlook 2013 or later */
    int do_read64;
    /** file offset of the first b-tree node in the index tree */
    uint64_t index1;
    /** back pointer value in the first b-tree node in the index tree */
    uint64_t index1_back;
    /** file offset of the first b-tree node in the descriptor tree*/
    uint64_t index2;
    /** back pointer value in the first b-tree node in the descriptor tree */
    uint64_t index2_back;
    /** size of the pst file */
    uint64_t size;
    /** @li 0 PST_NO_ENCRYPT, none
     *  @li 1 PST_COMP_ENCRYPT, simple byte substitution cipher with fixed key
     *  @li 2 PST_ENCRYPT, german enigma 3 rotor cipher with fixed key */
    unsigned char encryption;
    /** index type or file type
     *  @li 0x0e 32 bit pre Outlook 2003
     *  @li 0x0f 32 bit pre Outlook 2003
     *  @li 0x15 64 bit Outlook 2003 or later
     *  @li 0x17 64 bit Outlook 2003 or later */
    unsigned char ind_type;
} pst_file;


/** Open a pst file.
 * @param pf       pointer to uninitialized pst_file structure. This structure
 *                 will be filled in by this function.
 * @param name     name of the file, suitable for fopen().
 * @param charset  default charset for item with unspecified character sets
 * @return 0 if ok, -1 if error
 */
int             pst_open(pst_file *pf, const char *name, const char *charset);


/** Reopen the pst file after a fork
 * @param pf   pointer to the pst_file structure setup by pst_open().
 * @return 0 if ok, -1 if error
 */
int             pst_reopen(pst_file *pf);


/** Load the index entries from the pst file. This loads both the
 *  i_id linked list, and the d_id tree, and should normally be the
 *  first call after pst_open().
 * @param pf pointer to the pst_file structure setup by pst_open().
 */
int             pst_load_index (pst_file *pf);


/** Load the extended attribute mapping table from the pst file. This
 *  should normally be the second call after pst_open().
 * @param pf pointer to the pst_file structure setup by pst_open().
 */
int             pst_load_extended_attributes(pst_file *pf);


/** Close a pst file.
 * @param pf pointer to the pst_file structure setup by pst_open().
 */
int             pst_close(pst_file *pf);


/** Get the top of folders descriptor tree. This is the main descriptor tree
 *  that needs to be walked to look at every item in the pst file.
 * @param pf   pointer to the pst_file structure setup by pst_open().
 * @param root root item, which can be obtained by pst_parse_item(pf, pf->d_head, NULL).
 */
pst_desc_tree*  pst_getTopOfFolders(pst_file *pf, const pst_item *root);


/** Assemble the binary attachment into a single buffer.
 * @param pf     pointer to the pst_file structure setup by pst_open().
 * @param attach pointer to the attachment record
 * @return       structure containing size of and pointer to the buffer.
 *               the caller must free this buffer.
 */
pst_binary      pst_attach_to_mem(pst_file *pf, pst_item_attach *attach);


/** Write a binary attachment to a file.
 * @param pf     pointer to the pst_file structure setup by pst_open().
 * @param attach pointer to the attachment record
 * @param fp     pointer to an open FILE.
 */
size_t          pst_attach_to_file(pst_file *pf, pst_item_attach *attach, FILE* fp);


/** Write a binary attachment base64 encoded to a file.
 * @param pf     pointer to the pst_file structure setup by pst_open().
 * @param attach pointer to the attachment record
 * @param fp     pointer to an open FILE.
 */
size_t          pst_attach_to_file_base64(pst_file *pf, pst_item_attach *attach, FILE* fp);


/** Walk the descriptor tree.
 * @param d pointer to the current item in the descriptor tree.
 * @return  pointer to the next item in the descriptor tree.
 */
pst_desc_tree*  pst_getNextDptr(pst_desc_tree* d);


/** Assemble a mapi object from a descriptor pointer.
 * @param pf     pointer to the pst_file structure setup by pst_open().
 * @param d_ptr  pointer to an item in the descriptor tree.
 * @param m_head normally NULL. This is only used when processing embedded
 *               attached rfc822 messages, in which case it is attach->id2_head.
 * @return pointer to the mapi object. Must be free'd by pst_freeItem().
 */
pst_item*       pst_parse_item (pst_file *pf, pst_desc_tree *d_ptr, pst_id2_tree *m_head);


/** Free the item returned by pst_parse_item().
 * @param item  pointer to item returned from pst_parse_item().
 */
void            pst_freeItem(pst_item *item);


/** Lookup the i_id in the index linked list, and return a pointer to the element.
 * @param pf     pointer to the pst_file structure setup by pst_open().
 * @param i_id   key for the index linked list
 * @return pointer to the element, or NULL if not found.
 */
pst_index_ll*   pst_getID(pst_file* pf, uint64_t i_id);


/** Get an ID block from the file using pst_ff_getIDblock() and decrypt if necessary.
 * @param pf   pointer to the pst_file structure setup by pst_open().
 * @param i_id ID of block to retrieve
 * @param buf  reference to pointer to buffer that will contain the data block.
 *             If this pointer is non-NULL, it will first be free()d.
 * @return     Size of block read into memory
 */
size_t          pst_ff_getIDblock_dec(pst_file *pf, uint64_t i_id, char **buf);


/** compare strings case-insensitive.
 *  @return  -1 if a < b, 0 if a==b, 1 if a > b
 */
int pst_stricmp(char *a, char *b);


/** fwrite with checking for null pointer.
 * @param ptr pointer to the buffer
 * @param size  size of each item
 * @param nmemb number of items
 * @param stream output file
 * @return number of bytes written, zero if ptr==NULL
 */
size_t          pst_fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);


/** Add any necessary escape characters for rfc2426 vcard format
 * @param[in]     str       pointer to input string
 * @param[in,out] result    pointer to a char* pointer that may be realloc'ed if needed
 * @param[in,out] resultlen size of the result buffer
 * @return    pointer to output string, either the input pointer if
 *            there are no characters that need escapes, or a pointer
 *            to a possibly realloc'ed result buffer.
 */
char*           pst_rfc2426_escape(char* str, char** result, size_t* resultlen);


/** Convert a FILETIME into rfc2425 date/time format 1953-10-15T23:10:00Z
 *  which is the same as one of the forms in the ISO3601 standard
 * @param[in]  ft      time to be converted
 * @param[in]  buflen  length of the output buffer
 * @param[out] result  pointer to output buffer, must be at least 30 bytes
 * @return   time in rfc2425 format
 */
char*           pst_rfc2425_datetime_format(const FILETIME* ft, int buflen, char* result);


/** Convert a FILETIME into rfc2445 date/time format 19531015T231000Z
 * @param[in]  ft      time to be converted
 * @param[in]  buflen  length of the output buffer
 * @param[out] result  pointer to output buffer, must be at least 30 bytes
 * @return   time in rfc2445 format
 */
char*           pst_rfc2445_datetime_format(const FILETIME* ft, int buflen, char* result);


/** Convert the current time rfc2445 date/time format 19531015T231000Z
 * @param[in]  buflen  length of the output buffer
 * @param[out] result  pointer to output buffer, must be at least 30 bytes
 * @return   time in rfc2445 format
 */
char*           pst_rfc2445_datetime_format_now(int buflen, char* result);


/** Get the default character set for this item. This is used to find
 *  the charset for pst_string elements that are not already in utf8 encoding.
 * @param      item    pointer to the mapi item of interest
 * @param[in]  buflen  length of the output buffer
 * @param[out] result  pointer to output buffer, must be at least 30 bytes
 * @return default character set as a string useable by iconv()
 */
const char*     pst_default_charset(pst_item *item, int buflen, char* result);


/** Convert str to rfc2231 encoding of str
 *  @param str   pointer to the mapi string of interest
 */
void            pst_rfc2231(pst_string *str);


/** Convert str to rfc2047 encoding of str, possibly enclosed in quotes if it contains spaces
 *  @param item          pointer to the containing mapi item
 *  @param str           pointer to the mapi string of interest
 *  @param needs_quote   true if strings containing spaces should be wrapped in quotes
 */
void            pst_rfc2047(pst_item *item, pst_string *str, int needs_quote);


/** Convert str to utf8 if possible; null strings are preserved.
 * @param item  pointer to the containing mapi item
 * @param str   pointer to the mapi string of interest
 */
void            pst_convert_utf8_null(pst_item *item, pst_string *str);


/** Convert str to utf8 if possible; null strings are converted into empty strings.
 * @param item  pointer to the containing mapi item
 * @param str   pointer to the mapi string of interest
 */
void            pst_convert_utf8(pst_item *item, pst_string *str);


/** Decode raw recurrence data into a better structure.
 * @param appt pointer to appointment structure
 * @return     pointer to decoded recurrence structure that must be free'd by the caller.
 */
pst_recurrence* pst_convert_recurrence(pst_item_appointment* appt);


/** Free a recurrence structure.
 * @param r input pointer to be freed
 */
void            pst_free_recurrence(pst_recurrence* r);



// switch from maximal packing back to default packing
// undo the packing from the beginning of this file
#ifdef _MSC_VER
    #pragma pack(pop)
#endif
#if defined(__GNUC__) || defined (__SUNPRO_C) || defined(__SUNPRO_CC)
    #pragma pack()
#endif



#endif
