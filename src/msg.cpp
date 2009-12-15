extern "C" {
    #include "define.h"
    #include "msg.h"
    #include <gsf/gsf-utils.h>

    #include <gsf/gsf-input-stdio.h>
    #include <gsf/gsf-infile.h>
    #include <gsf/gsf-infile-stdio.h>

    #include <gsf/gsf-output-stdio.h>
    #include <gsf/gsf-outfile.h>
    #include <gsf/gsf-outfile-msole.h>
}

#include <list>
#include <vector>
#include <string>

using namespace std;

struct property {
    uint32_t  tag;
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


static void empty_property(GsfOutfile *out, uint32_t tag);
static void empty_property(GsfOutfile *out, uint32_t tag) {
    vector<char> n(50);
    snprintf(&n[0], n.size(), "__substg1.0_%08X", tag);
    GsfOutput* dst = gsf_outfile_new_child(out, &n[0], false);
    gsf_output_close(dst);
    g_object_unref(G_OBJECT(dst));
}


static void string_property(GsfOutfile *out, property_list &prop, uint32_t tag, const char *contents, size_t size);
static void string_property(GsfOutfile *out, property_list &prop, uint32_t tag, const char *contents, size_t size) {
    if (!contents) return;
    vector<char> n(50);
    snprintf(&n[0], n.size(), "__substg1.0_%08X", tag);
    GsfOutput* dst = gsf_outfile_new_child(out, &n[0], false);
    gsf_output_write(dst, size, (const guint8*)contents);
    gsf_output_close(dst);
    g_object_unref(G_OBJECT(dst));

    int bias = ((tag & 0x0000ffff) == 0x001e) ? 1 : 0;
    property p;
    p.tag      = tag;
    p.flags    = 0x6;   // make all the properties writable
    p.length   = bias + size;
    p.reserved = 0;
    prop.push_back(p);
}


static void string_property(GsfOutfile *out, property_list &prop, uint32_t tag, FILE *fp);
static void string_property(GsfOutfile *out, property_list &prop, uint32_t tag, FILE *fp) {
    vector<char> n(50);
    snprintf(&n[0], n.size(), "__substg1.0_%08X", tag);
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
    p.tag      = tag;
    p.flags    = 0x6;   // make all the properties writable
    p.length   = size;
    p.reserved = 0;
    prop.push_back(p);
}


static void string_property(GsfOutfile *out, property_list &prop, uint32_t tag, const char* charset, pst_string &contents);
static void string_property(GsfOutfile *out, property_list &prop, uint32_t tag, const char* charset, pst_string &contents) {
    if (contents.str) {
        convert_8bit(contents, charset);
        string_property(out, prop, tag, contents.str, strlen(contents.str));
    }
}


static void strin0_property(GsfOutfile *out, property_list &prop, uint32_t tag, const char* charset, pst_string &contents);
static void strin0_property(GsfOutfile *out, property_list &prop, uint32_t tag, const char* charset, pst_string &contents) {
    if (contents.str) {
        convert_8bit(contents, charset);
        string_property(out, prop, tag, contents.str, strlen(contents.str)+1);
    }
}


static void string_property(GsfOutfile *out, property_list &prop, uint32_t tag, const string &contents);
static void string_property(GsfOutfile *out, property_list &prop, uint32_t tag, const string &contents) {
    string_property(out, prop, tag, contents.c_str(), contents.size());
}


static void string_property(GsfOutfile *out, property_list &prop, uint32_t tag, pst_binary &contents);
static void string_property(GsfOutfile *out, property_list &prop, uint32_t tag, pst_binary &contents) {
    if (contents.size) string_property(out, prop, tag, contents.data, contents.size);
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


static void int_property(property_list &prop_list, uint32_t tag, uint32_t flags, uint32_t value);
static void int_property(property_list &prop_list, uint32_t tag, uint32_t flags, uint32_t value) {
    property p;
    p.tag      = tag;
    p.flags    = flags;
    p.length   = value;
    p.reserved = 0;
    prop_list.push_back(p);
}


static void nzi_property(property_list &prop_list, uint32_t tag, uint32_t flags, uint32_t value);
static void nzi_property(property_list &prop_list, uint32_t tag, uint32_t flags, uint32_t value) {
    if (value) int_property(prop_list, tag, flags, value);
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

    int_property(prop_list, 0x00170003, 0x6, email.importance);
    nzi_property(prop_list, 0x0023000B, 0x6, email.delivery_report);
    nzi_property(prop_list, 0x00260003, 0x6, email.priority);
    nzi_property(prop_list, 0x0029000B, 0x6, email.read_receipt);
    nzi_property(prop_list, 0x002E0003, 0x6, email.original_sensitivity);
    nzi_property(prop_list, 0x00360003, 0x6, email.sensitivity);
    nzi_property(prop_list, 0x0C17000B, 0x6, email.reply_requested);
    nzi_property(prop_list, 0x0E01000B, 0x6, email.delete_after_submit);
    int_property(prop_list, 0x0E070003, 0x6, item->flags);
    GsfOutfile *out = GSF_OUTFILE (output);
    string_property(out, prop_list, 0x001A001E, item->ascii_type);
    string_property(out, prop_list, 0x0037001E, body_charset, item->subject);
    strin0_property(out, prop_list, 0x003B0102, body_charset, email.outlook_sender);
    string_property(out, prop_list, 0x003D001E, string(""));
    string_property(out, prop_list, 0x0040001E, body_charset, email.outlook_received_name1);
    string_property(out, prop_list, 0x0042001E, body_charset, email.outlook_sender_name);
    string_property(out, prop_list, 0x0044001E, body_charset, email.outlook_recipient_name);
    string_property(out, prop_list, 0x0050001E, body_charset, email.reply_to);
    strin0_property(out, prop_list, 0x00510102, body_charset, email.outlook_recipient);
    strin0_property(out, prop_list, 0x00520102, body_charset, email.outlook_recipient2);
    string_property(out, prop_list, 0x0064001E, body_charset, email.sender_access);
    string_property(out, prop_list, 0x0065001E, body_charset, email.sender_address);
    string_property(out, prop_list, 0x0070001E, body_charset, email.processed_subject);
    string_property(out, prop_list, 0x00710102,               email.conversation_index);
    string_property(out, prop_list, 0x0072001E, body_charset, email.original_bcc);
    string_property(out, prop_list, 0x0073001E, body_charset, email.original_cc);
    string_property(out, prop_list, 0x0074001E, body_charset, email.original_to);
    string_property(out, prop_list, 0x0075001E, body_charset, email.recip_access);
    string_property(out, prop_list, 0x0076001E, body_charset, email.recip_address);
    string_property(out, prop_list, 0x0077001E, body_charset, email.recip2_access);
    string_property(out, prop_list, 0x0078001E, body_charset, email.recip2_address);
    string_property(out, prop_list, 0x007D001E, body_charset, email.header);
    string_property(out, prop_list, 0x0C1A001E, body_charset, email.outlook_sender_name2);
    strin0_property(out, prop_list, 0x0C1D0102, body_charset, email.outlook_sender2);
    string_property(out, prop_list, 0x0C1E001E, body_charset, email.sender2_access);
    string_property(out, prop_list, 0x0C1F001E, body_charset, email.sender2_address);
    string_property(out, prop_list, 0x0E02001E, body_charset, email.bcc_address);
    string_property(out, prop_list, 0x0E03001E, body_charset, email.cc_address);
    string_property(out, prop_list, 0x0E04001E, body_charset, email.sentto_address);
    string_property(out, prop_list, 0x0E1D001E, body_charset, email.outlook_normalized_subject);
    string_property(out, prop_list, 0x1000001E, body_charset, item->body);
    string_property(out, prop_list, 0x1013001E, body_charset, email.htmlbody);
    string_property(out, prop_list, 0x1035001E, body_charset, email.messageid);
    string_property(out, prop_list, 0x1042001E, body_charset, email.in_reply_to);
    string_property(out, prop_list, 0x1046001E, body_charset, email.return_path_address);
    // any property over 0x8000 needs entries in the __nameid to make them
    // either string named or numerical named properties.

    {
        vector<char> n(50);
        snprintf(&n[0], n.size(), "__recip_version1.0_#%08X", top_head.recipient_count);
        GsfOutput  *output = gsf_outfile_new_child(out, &n[0], true);
        {
            int v = (email.message_recip_me) ? 1 :  // to
                    (email.message_cc_me)    ? 2 :  // cc
                                               3;   // bcc
            property_list prop_list;
            int_property(prop_list, 0x0C150003, 0x6, v);                        // PidTagRecipientType
            int_property(prop_list, 0x30000003, 0x6, top_head.recipient_count); // PR_ROWID
            GsfOutfile *out = GSF_OUTFILE (output);
            string_property(out, prop_list, 0x3001001E, body_charset, item->file_as);
            if (item->contact) {
                string_property(out, prop_list, 0x3002001E, body_charset, item->contact->address1_transport);
                string_property(out, prop_list, 0x3003001E, body_charset, item->contact->address1);
            }
            strin0_property(out, prop_list, 0x300B0102, body_charset, email.outlook_search_key);
            write_properties(out, prop_list, (const guint8*)&top_head, 8);  // convenient 8 bytes of reserved zeros
            gsf_output_close(output);
            g_object_unref(G_OBJECT(output));
            top_head.next_recipient++;
            top_head.recipient_count++;
        }
    }

    pst_item_attach *a = item->attach;
    while (a) {
        if (a->method == PST_ATTACH_EMBEDDED) {
            // not implemented yet
        }
        else if (a->data.data || a->i_id) {
            vector<char> n(50);
            snprintf(&n[0], n.size(), "__attach_version1.0_#%08X", top_head.attachment_count);
            GsfOutput  *output = gsf_outfile_new_child(out, &n[0], true);
            {
                FILE *fp = fopen("temp_file_attachment", "w+b");
                if (fp) {
                    pst_attach_to_file(pst, a, fp); // data is now in the file
                    fseek(fp, 0, SEEK_SET);
                    property_list prop_list;
                    int_property(prop_list, 0x0E210003, 0x2, top_head.attachment_count);    // MAPI_ATTACH_NUM
                    int_property(prop_list, 0x0FF40003, 0x2, 2);            // PR_ACCESS read
                    int_property(prop_list, 0x0FF70003, 0x2, 0);            // PR_ACCESS_LEVEL read only
                    int_property(prop_list, 0x0FFE0003, 0x2, 7);            // PR_OBJECT_TYPE attachment
                    int_property(prop_list, 0x37050003, 0x7, 1);            // PR_ATTACH_METHOD by value
                    int_property(prop_list, 0x370B0003, 0x7, a->position);  // PR_RENDERING_POSITION
                    int_property(prop_list, 0x37100003, 0x6, a->sequence);  // PR_ATTACH_MIME_SEQUENCE
                    GsfOutfile *out = GSF_OUTFILE (output);
                    string_property(out, prop_list, 0x0FF90102, item->record_key);
                    string_property(out, prop_list, 0x37010102, fp);
                    string_property(out, prop_list, 0x3704001E, body_charset, a->filename1);
                    string_property(out, prop_list, 0x3707001E, body_charset, a->filename2);
                    string_property(out, prop_list, 0x370E001E, body_charset, a->mimetype);
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

    {
        GsfOutput  *output = gsf_outfile_new_child(out, "__nameid_version1.0", true);
        {
            GsfOutfile *out = GSF_OUTFILE (output);
            empty_property(out, 0x00020102);
            empty_property(out, 0x00030102);
            empty_property(out, 0x00040102);
            gsf_output_close(output);
            g_object_unref(G_OBJECT(output));
        }
    }

    write_properties(out, prop_list, (const guint8*)&top_head, sizeof(top_head));
    gsf_output_close(output);
    g_object_unref(G_OBJECT(output));

    gsf_shutdown();
    DEBUG_RET();
}

