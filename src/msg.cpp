#include "define.h"
#include "msg.h"

#include <gsf/gsf-utils.h>

#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-infile-stdio.h>

#include <gsf/gsf-output-stdio.h>
#include <gsf/gsf-outfile.h>
#include <gsf/gsf-outfile-msole.h>

#include <list>
#include <vector>
#include <string>

using namespace std;

struct property {
    uint16_t  type;
    uint16_t  id;
    uint32_t  flags;
    uint32_t  length; // or value
    uint32_t  reserved;
};
typedef list<property> property_list;


/** Convert str to an 8 bit charset if it is utf8, null strings are preserved.
 *
 *  @param str     reference to the mapi string of interest
 *  @param charset pointer to the 8 bit charset to use
 */
static void convert_8bit(pst_string &str, const char *charset);
static void convert_8bit(pst_string &str, const char *charset) {
    if (!str.str)     return;  // null
    if (!str.is_utf8) return;  // not utf8

    DEBUG_ENT("convert_8bit");
    pst_vbuf *newer = pst_vballoc(2);
    size_t strsize = strlen(str.str);
    size_t rc = pst_vb_utf8to8bit(newer, str.str, strsize, charset);
    if (rc == (size_t)-1) {
        // unable to convert, change the charset to utf8
        free(newer->b);
        DEBUG_INFO(("Failed to convert utf-8 to %s\n", charset));
        DEBUG_HEXDUMPC(str.str, strsize, 0x10);
    }
    else {
        // null terminate the output string
        pst_vbgrow(newer, 1);
        newer->b[newer->dlen] = '\0';
        free(str.str);
        str.str = newer->b;
    }
    free(newer);
    DEBUG_RET();
}


static void empty_property(GsfOutfile *out, uint16_t id, uint16_t type);
static void empty_property(GsfOutfile *out, uint16_t id, uint16_t type) {
    vector<char> n(50);
    snprintf(&n[0], n.size(), "__substg1.0_%04" PRIX32 "%04" PRIX32, id, type);
    GsfOutput* dst = gsf_outfile_new_child(out, &n[0], false);
    gsf_output_close(dst);
    g_object_unref(G_OBJECT(dst));
}


static void string_property(GsfOutfile *out, property_list &prop, uint16_t id, uint16_t type, const char *contents, size_t size);
static void string_property(GsfOutfile *out, property_list &prop, uint16_t id, uint16_t type, const char *contents, size_t size) {
    if (!contents) return;
    size_t term = (type == 0x001e) ? 1 :
                  (type == 0x001f) ? 2 : 0;  // null terminator
    vector<char> n(50);
    snprintf(&n[0], n.size(), "__substg1.0_%04" PRIX32 "%04" PRIX32, id, type);
    GsfOutput* dst = gsf_outfile_new_child(out, &n[0], false);
    gsf_output_write(dst, size, (const guint8*)contents);
    if (term) {
        memset(&n[0], 0, term);
        gsf_output_write(dst, term, (const guint8*)&n[0]);
        size += term;
    }
    gsf_output_close(dst);
    g_object_unref(G_OBJECT(dst));

    property p;
    p.id       = id;
    p.type     = type;
    p.flags    = 0x6;   // make all the properties writable
    p.length   = size;
    p.reserved = 0;
    prop.push_back(p);
}


static void string_property(GsfOutfile *out, property_list &prop, uint16_t id, uint16_t type, FILE *fp);
static void string_property(GsfOutfile *out, property_list &prop, uint16_t id, uint16_t type, FILE *fp) {
    vector<char> n(50);
    snprintf(&n[0], n.size(), "__substg1.0_%04" PRIX32 "%04" PRIX32, id, type);
    GsfOutput* dst = gsf_outfile_new_child(out, &n[0], false);

    size_t size = 0;
    const size_t bsize = 10000;
    char buf[bsize];

    while (1) {
        size_t s = fread(buf, 1, bsize, fp);
        if (!s) break;
        gsf_output_write(dst, s, (const guint8*)buf);
    }

    gsf_output_close(dst);
    g_object_unref(G_OBJECT(dst));

    property p;
    p.id       = id;
    p.type     = type;
    p.flags    = 0x6;   // make all the properties writable
    p.length   = size;
    p.reserved = 0;
    prop.push_back(p);
}


static void string_property(GsfOutfile *out, property_list &prop, uint16_t id, uint16_t type, const char* charset, pst_string &contents);
static void string_property(GsfOutfile *out, property_list &prop, uint16_t id, uint16_t type, const char* charset, pst_string &contents) {
    if (contents.str) {
        convert_8bit(contents, charset);
        string_property(out, prop, id, type, contents.str, strlen(contents.str));
    }
}


static void strin0_property(GsfOutfile *out, property_list &prop, uint16_t id, uint16_t type, const char* charset, pst_string &contents);
static void strin0_property(GsfOutfile *out, property_list &prop, uint16_t id, uint16_t type, const char* charset, pst_string &contents) {
    if (contents.str) {
        convert_8bit(contents, charset);
        string_property(out, prop, id, type, contents.str, strlen(contents.str)+1);
    }
}


static void string_property(GsfOutfile *out, property_list &prop, uint16_t id, uint16_t type, const string &contents);
static void string_property(GsfOutfile *out, property_list &prop, uint16_t id, uint16_t type, const string &contents) {
    string_property(out, prop, id, type, contents.c_str(), contents.size());
}


static void string_property(GsfOutfile *out, property_list &prop, uint16_t id, uint16_t type, pst_binary &contents);
static void string_property(GsfOutfile *out, property_list &prop, uint16_t id, uint16_t type, pst_binary &contents) {
    if (contents.size) string_property(out, prop, id, type, contents.data, contents.size);
}


static void write_properties(GsfOutfile *out, property_list &prop, const guint8* header, size_t hlen);
static void write_properties(GsfOutfile *out, property_list &prop, const guint8* header, size_t hlen) {
    GsfOutput* dst = gsf_outfile_new_child(out, "__properties_version1.0", false);
    gsf_output_write(dst, hlen, header);
    for (property_list::iterator i=prop.begin(); i!=prop.end(); i++) {
        property &p = *i;
        gsf_output_write(dst, sizeof(property), (const guint8*)&p);
    }
    gsf_output_close(dst);
    g_object_unref(G_OBJECT(dst));
}


static void int_property(property_list &prop_list, uint16_t id, uint16_t type, uint32_t flags, uint32_t value);
static void int_property(property_list &prop_list, uint16_t id, uint16_t type, uint32_t flags, uint32_t value) {
    property p;
    p.id       = id;
    p.type     = type;
    p.flags    = flags;
    p.length   = value;
    p.reserved = 0;
    prop_list.push_back(p);
}


static void i64_property(property_list &prop_list, uint16_t id, uint16_t type, uint32_t flags, FILETIME *value);
static void i64_property(property_list &prop_list, uint16_t id, uint16_t type, uint32_t flags, FILETIME *value) {
    if (value) {
        property p;
        p.id       = id;
        p.type     = type;
        p.flags    = flags;
        p.length   = value->dwLowDateTime;
        p.reserved = value->dwHighDateTime;
        prop_list.push_back(p);
    }
}


static void nzi_property(property_list &prop_list, uint16_t id, uint16_t type, uint32_t flags, uint32_t value);
static void nzi_property(property_list &prop_list, uint16_t id, uint16_t type, uint32_t flags, uint32_t value) {
    if (value) int_property(prop_list, id, type, flags, value);
}


void write_msg_email(char *fname, pst_item* item, pst_file* pst) {
    // this is not an email item
    if (!item->email) return;
    DEBUG_ENT("write_msg_email");

    pst_item_email &email = *(item->email);

    char charset[30];
    const char* body_charset = pst_default_charset(item, sizeof(charset), charset);
    DEBUG_INFO(("%s body charset seems to be %s\n", fname, body_charset));
    body_charset = "iso-8859-1//TRANSLIT//IGNORE";

    gsf_init();

    GsfOutfile *outfile;
    GsfOutput  *output;
    GError    *err = NULL;

    output = gsf_output_stdio_new(fname, &err);
    if (output == NULL) {
        gsf_shutdown();
        DEBUG_INFO(("unable to open output .msg file %s\n", fname));
        DEBUG_RET();
        return;
    }

    struct top_property_header {
        uint32_t  reserved1;
        uint32_t  reserved2;
        uint32_t  next_recipient;   // same as recipient count
        uint32_t  next_attachment;  // same as attachment count
        uint32_t  recipient_count;
        uint32_t  attachment_count;
        uint32_t  reserved3;
        uint32_t  reserved4;
    };

    top_property_header top_head;
    memset(&top_head, 0, sizeof(top_head));

    outfile = gsf_outfile_msole_new(output);
    g_object_unref(G_OBJECT(output));

    output = GSF_OUTPUT(outfile);
    property_list prop_list;

    int_property(prop_list, 0x0017, 0x0003, 0x6, email.importance);
    nzi_property(prop_list, 0x0023, 0x000B, 0x6, email.delivery_report);
    nzi_property(prop_list, 0x0026, 0x0003, 0x6, email.priority);
    nzi_property(prop_list, 0x0029, 0x000B, 0x6, email.read_receipt);
    nzi_property(prop_list, 0x002E, 0x0003, 0x6, email.original_sensitivity);
    nzi_property(prop_list, 0x0036, 0x0003, 0x6, email.sensitivity);
    nzi_property(prop_list, 0x0C17, 0x000B, 0x6, email.reply_requested);
    nzi_property(prop_list, 0x0E01, 0x000B, 0x6, email.delete_after_submit);
    int_property(prop_list, 0x0E07, 0x0003, 0x6, item->flags);
    i64_property(prop_list, 0x0039, 0x0040, 0x6, email.sent_date);
    GsfOutfile *out = GSF_OUTFILE (output);
    string_property(out, prop_list, 0x001A, 0x001E, item->ascii_type);
    string_property(out, prop_list, 0x0037, 0x001E, body_charset, item->subject);
    strin0_property(out, prop_list, 0x003B, 0x0102, body_charset, email.outlook_sender);
    string_property(out, prop_list, 0x003D, 0x001E, string(""));
    string_property(out, prop_list, 0x0040, 0x001E, body_charset, email.outlook_received_name1);
    string_property(out, prop_list, 0x0042, 0x001E, body_charset, email.outlook_sender_name);
    string_property(out, prop_list, 0x0044, 0x001E, body_charset, email.outlook_recipient_name);
    string_property(out, prop_list, 0x0050, 0x001E, body_charset, email.reply_to);
    strin0_property(out, prop_list, 0x0051, 0x0102, body_charset, email.outlook_recipient);
    strin0_property(out, prop_list, 0x0052, 0x0102, body_charset, email.outlook_recipient2);
    string_property(out, prop_list, 0x0064, 0x001E, body_charset, email.sender_access);
    string_property(out, prop_list, 0x0065, 0x001E, body_charset, email.sender_address);
    string_property(out, prop_list, 0x0070, 0x001E, body_charset, email.processed_subject);
    string_property(out, prop_list, 0x0071, 0x0102,               email.conversation_index);
    string_property(out, prop_list, 0x0072, 0x001E, body_charset, email.original_bcc);
    string_property(out, prop_list, 0x0073, 0x001E, body_charset, email.original_cc);
    string_property(out, prop_list, 0x0074, 0x001E, body_charset, email.original_to);
    string_property(out, prop_list, 0x0075, 0x001E, body_charset, email.recip_access);
    string_property(out, prop_list, 0x0076, 0x001E, body_charset, email.recip_address);
    string_property(out, prop_list, 0x0077, 0x001E, body_charset, email.recip2_access);
    string_property(out, prop_list, 0x0078, 0x001E, body_charset, email.recip2_address);
    string_property(out, prop_list, 0x007D, 0x001E, body_charset, email.header);
    string_property(out, prop_list, 0x0C1A, 0x001E, body_charset, email.outlook_sender_name2);
    strin0_property(out, prop_list, 0x0C1D, 0x0102, body_charset, email.outlook_sender2);
    string_property(out, prop_list, 0x0C1E, 0x001E, body_charset, email.sender2_access);
    string_property(out, prop_list, 0x0C1F, 0x001E, body_charset, email.sender2_address);
    string_property(out, prop_list, 0x0E02, 0x001E, body_charset, email.bcc_address);
    string_property(out, prop_list, 0x0E03, 0x001E, body_charset, email.cc_address);
    string_property(out, prop_list, 0x0E04, 0x001E, body_charset, email.sentto_address);
    string_property(out, prop_list, 0x0E1D, 0x001E, body_charset, email.outlook_normalized_subject);
    string_property(out, prop_list, 0x1000, 0x001E, body_charset, item->body);
    string_property(out, prop_list, 0x1013, 0x001E, body_charset, email.htmlbody);
    string_property(out, prop_list, 0x1035, 0x001E, body_charset, email.messageid);
    string_property(out, prop_list, 0x1042, 0x001E, body_charset, email.in_reply_to);
    string_property(out, prop_list, 0x1046, 0x001E, body_charset, email.return_path_address);
    // any property over 0x8000 needs entries in the __nameid to make them
    // either string named or numerical named properties.

    {
        vector<char> n(50);
        {
            snprintf(&n[0], n.size(), "__recip_version1.0_#%08" PRIX32, top_head.recipient_count);
            GsfOutput  *output = gsf_outfile_new_child(out, &n[0], true);
            {
                int v = 1;  // to
                property_list prop_list;
                int_property(prop_list, 0x0C15, 0x0003, 0x6, v);                        // PidTagRecipientType
                int_property(prop_list, 0x3000, 0x0003, 0x6, top_head.recipient_count); // PR_ROWID
                GsfOutfile *out = GSF_OUTFILE (output);
                string_property(out, prop_list, 0x3001, 0x001E, body_charset, item->file_as);
                if (item->contact) {
                    string_property(out, prop_list, 0x3002, 0x001E, body_charset, item->contact->address1_transport);
                    string_property(out, prop_list, 0x3003, 0x001E, body_charset, item->contact->address1);
                    string_property(out, prop_list, 0x5ff6, 0x001E, body_charset, item->contact->address1);
                }
                strin0_property(out, prop_list, 0x300B, 0x0102, body_charset, email.outlook_search_key);
                write_properties(out, prop_list, (const guint8*)&top_head, 8);  // convenient 8 bytes of reserved zeros
                gsf_output_close(output);
                g_object_unref(G_OBJECT(output));
                top_head.next_recipient++;
                top_head.recipient_count++;
            }
        }
        if (email.cc_address.str) {
            snprintf(&n[0], n.size(), "__recip_version1.0_#%08" PRIX32, top_head.recipient_count);
            GsfOutput  *output = gsf_outfile_new_child(out, &n[0], true);
            {
                int v = 2;  // cc
                property_list prop_list;
                int_property(prop_list, 0x0C15, 0x0003, 0x6, v);                        // PidTagRecipientType
                int_property(prop_list, 0x3000, 0x0003, 0x6, top_head.recipient_count); // PR_ROWID
                GsfOutfile *out = GSF_OUTFILE (output);
                string_property(out, prop_list, 0x3001, 0x001E, body_charset, email.cc_address);
                string_property(out, prop_list, 0x3003, 0x001E, body_charset, email.cc_address);
                string_property(out, prop_list, 0x5ff6, 0x001E, body_charset, email.cc_address);
                write_properties(out, prop_list, (const guint8*)&top_head, 8);  // convenient 8 bytes of reserved zeros
                gsf_output_close(output);
                g_object_unref(G_OBJECT(output));
                top_head.next_recipient++;
                top_head.recipient_count++;
            }
        }
        if (email.bcc_address.str) {
            snprintf(&n[0], n.size(), "__recip_version1.0_#%08" PRIX32, top_head.recipient_count);
            GsfOutput  *output = gsf_outfile_new_child(out, &n[0], true);
            {
                int v = 3;  // bcc
                property_list prop_list;
                int_property(prop_list, 0x0C15, 0x0003, 0x6, v);                        // PidTagRecipientType
                int_property(prop_list, 0x3000, 0x0003, 0x6, top_head.recipient_count); // PR_ROWID
                GsfOutfile *out = GSF_OUTFILE (output);
                string_property(out, prop_list, 0x3001, 0x001E, body_charset, email.bcc_address);
                string_property(out, prop_list, 0x3003, 0x001E, body_charset, email.bcc_address);
                string_property(out, prop_list, 0x5ff6, 0x001E, body_charset, email.bcc_address);
                write_properties(out, prop_list, (const guint8*)&top_head, 8);  // convenient 8 bytes of reserved zeros
                gsf_output_close(output);
                g_object_unref(G_OBJECT(output));
                top_head.next_recipient++;
                top_head.recipient_count++;
            }
        }
    }

    pst_item_attach *a = item->attach;
    while (a) {
        if (a->method == PST_ATTACH_EMBEDDED) {
            // not implemented yet
        }
        else if (a->data.data || a->i_id) {
            vector<char> n(50);
            snprintf(&n[0], n.size(), "__attach_version1.0_#%08" PRIX32, top_head.attachment_count);
            GsfOutput  *output = gsf_outfile_new_child(out, &n[0], true);
            {
                FILE *fp = fopen("temp_file_attachment", "w+b");
                if (fp) {
                    unlink("temp_file_attachment");
                    pst_attach_to_file(pst, a, fp); // data is now in the file
                    fseek(fp, 0, SEEK_SET);
                    property_list prop_list;
                    int_property(prop_list, 0x0E21, 0x0003, 0x2, top_head.attachment_count);    // MAPI_ATTACH_NUM
                    int_property(prop_list, 0x0FF4, 0x0003, 0x2, 2);            // PR_ACCESS read
                    int_property(prop_list, 0x0FF7, 0x0003, 0x2, 0);            // PR_ACCESS_LEVEL read only
                    int_property(prop_list, 0x0FFE, 0x0003, 0x2, 7);            // PR_OBJECT_TYPE attachment
                    int_property(prop_list, 0x3705, 0x0003, 0x7, 1);            // PR_ATTACH_METHOD by value
                    int_property(prop_list, 0x370B, 0x0003, 0x7, a->position);  // PR_RENDERING_POSITION
                    int_property(prop_list, 0x3710, 0x0003, 0x6, a->sequence);  // PR_ATTACH_MIME_SEQUENCE
                    GsfOutfile *out = GSF_OUTFILE (output);
                    string_property(out, prop_list, 0x0FF9, 0x0102, item->record_key);
                    string_property(out, prop_list, 0x3701, 0x0102, fp);
                    if (a->filename2.str) {
                        // have long file name
                        string_property(out, prop_list, 0x3707, 0x001E, body_charset, a->filename2);
                    }
                    else if (a->filename1.str) {
                        // have short file name
                        string_property(out, prop_list, 0x3704, 0x001E, body_charset, a->filename1);
                    }
                    else {
                        // make up a name
                        const char *n = "inline";
                        string_property(out, prop_list, 0x3704, 0x001E, n, strlen(n));
                    }
                    string_property(out, prop_list, 0x370E, 0x001E, body_charset, a->mimetype);
                    write_properties(out, prop_list, (const guint8*)&top_head, 8);  // convenient 8 bytes of reserved zeros
                    gsf_output_close(output);
                    g_object_unref(G_OBJECT(output));
                    top_head.next_attachment++;
                    top_head.attachment_count++;
                    fclose(fp);
                }
            }
        }
        a = a->next;
    }

    write_properties(out, prop_list, (const guint8*)&top_head, sizeof(top_head));

    {
        GsfOutput  *output = gsf_outfile_new_child(out, "__nameid_version1.0", true);
        {
            GsfOutfile *out = GSF_OUTFILE (output);
            empty_property(out, 0x0002, 0x0102);
            empty_property(out, 0x0003, 0x0102);
            empty_property(out, 0x0004, 0x0102);
            gsf_output_close(output);
            g_object_unref(G_OBJECT(output));
        }
    }

    gsf_output_close(output);
    g_object_unref(G_OBJECT(output));

    gsf_shutdown();
    DEBUG_RET();
}

