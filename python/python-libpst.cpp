#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#include <iostream>
#include <boost/python.hpp>
// #include <boost/python/docstring_options.hpp>
// #include <iostream>

extern "C" {
    #include "libpst.h"
    #include "timeconv.h"
    #include "libstrfunc.h"
    #include "vbuf.h"
    #include "lzfu.h"
}

using namespace std;
using namespace boost::python;


/** python version of pst_binary, where python is
    responsible for freeing the underlying buffer */
struct ppst_binary : public pst_binary
{
};

class pst {
public:
                    pst(const string filename, const string charset);
    virtual         ~pst();
    pst_desc_tree*  pst_getTopOfFolders();
    ppst_binary     pst_attach_to_mem(pst_item_attach *attach);
    size_t          pst_attach_to_file(pst_item_attach *attach, FILE* fp);
    size_t          pst_attach_to_file_base64(pst_item_attach *attach, FILE* fp);
    pst_desc_tree*  pst_getNextDptr(pst_desc_tree* d);
    pst_item*       pst_parse_item(pst_desc_tree *d_ptr, pst_id2_tree *m_head);
    void            pst_freeItem(pst_item *item);
    pst_index_ll*   pst_getID(uint64_t i_id);
    size_t          pst_ff_getIDblock_dec(uint64_t i_id, char **buf);
    string          pst_rfc2426_escape(char *str);
    string          pst_rfc2425_datetime_format(const FILETIME *ft);
    string          pst_rfc2445_datetime_format(const FILETIME *ft);
    string          pst_default_charset(pst_item *item);
    void            pst_convert_utf8_null(pst_item *item, pst_string *str);
    void            pst_convert_utf8(pst_item *item, pst_string *str);
    pst_recurrence* pst_convert_recurrence(pst_item_appointment *appt);
    void            pst_free_recurrence(pst_recurrence* r);

    /** helper for python access to fopen() */
    FILE*          ppst_open_file(string filename, string mode);
    /** helper for python access to fclose() */
    int            ppst_close_file(FILE* fp);

private:
    bool            is_open;
    pst_file        pf;
    pst_item*       root;
    pst_desc_tree*  topf;

};


pst::pst(const string filename, const string charset) {
    is_open = (::pst_open(&pf, filename.c_str(), charset.c_str()) == 0);
    root = NULL;
    topf = NULL;
    if (is_open) {
        ::pst_load_index(&pf);
        ::pst_load_extended_attributes(&pf);
        if (pf.d_head) root = ::pst_parse_item(&pf, pf.d_head, NULL);
        if (root)      topf = ::pst_getTopOfFolders(&pf, root)->child;
    }
}

pst::~pst() {
    if (root) pst_freeItem(root);
    if (is_open) ::pst_close(&pf);
}

pst_desc_tree*  pst::pst_getTopOfFolders() {
    return topf;
}

ppst_binary     pst::pst_attach_to_mem(pst_item_attach *attach) {
    pst_binary r = ::pst_attach_to_mem(&pf, attach);
    ppst_binary rc;
    rc.size = r.size;
    rc.data = r.data;
    return rc;
}

size_t          pst::pst_attach_to_file(pst_item_attach *attach, FILE* fp) {
    return ::pst_attach_to_file(&pf, attach, fp);
}

size_t          pst::pst_attach_to_file_base64(pst_item_attach *attach, FILE* fp) {
    return ::pst_attach_to_file_base64(&pf, attach, fp);
}

pst_desc_tree*  pst::pst_getNextDptr(pst_desc_tree* d) {
    return ::pst_getNextDptr(d);
}

pst_item*       pst::pst_parse_item (pst_desc_tree *d_ptr, pst_id2_tree *m_head) {
    return ::pst_parse_item(&pf, d_ptr, m_head);
}

void            pst::pst_freeItem(pst_item *item) {
    return ::pst_freeItem(item);
}

pst_index_ll*   pst::pst_getID(uint64_t i_id) {
    return ::pst_getID(&pf, i_id);
}

size_t          pst::pst_ff_getIDblock_dec(uint64_t i_id, char **buf) {
    return ::pst_ff_getIDblock_dec(&pf, i_id, buf);
}

string          pst::pst_rfc2426_escape(char *str) {
    char  *result = NULL;
    size_t resultlen = 0;
    char  *rc = ::pst_rfc2426_escape(str, &result, &resultlen);
    string rrc(rc);
    if (result) free(result);
    return rrc;
}

string          pst::pst_rfc2425_datetime_format(const FILETIME *ft) {
    char buf[30];
    ::pst_rfc2425_datetime_format(ft, sizeof(buf), buf);
    return string(buf);
}

string          pst::pst_rfc2445_datetime_format(const FILETIME *ft) {
    char buf[30];
    ::pst_rfc2445_datetime_format(ft, sizeof(buf), buf);
    return string(buf);
}

string          pst::pst_default_charset(pst_item *item) {
    char buf[30];
    return string(::pst_default_charset(item, sizeof(buf), buf));
}

void            pst::pst_convert_utf8_null(pst_item *item, pst_string *str) {
    ::pst_convert_utf8_null(item, str);
}

void            pst::pst_convert_utf8(pst_item *item, pst_string *str) {
    ::pst_convert_utf8(item, str);
}

pst_recurrence* pst::pst_convert_recurrence(pst_item_appointment *appt)
{
    return ::pst_convert_recurrence(appt);
}

void            pst::pst_free_recurrence(pst_recurrence* r)
{
    ::pst_free_recurrence(r);
}

FILE*          pst::ppst_open_file(string filename, string mode) {
    return ::fopen(filename.c_str(), mode.c_str());
}

int            pst::ppst_close_file(FILE* fp) {
    return ::fclose(fp);
}

struct make_python_string {
    /* we make no distinction between empty and null strings */
    static PyObject* convert(char* const &s) {
        string ss;
        if (s) ss = string(s);
        return boost::python::incref(boost::python::object(ss).ptr());
    }
};

struct make_python_pst_binary {
    static PyObject* convert(pst_binary const &s) {
        if (s.data) {
            string ss;
            ss = string(s.data, s.size);
            return boost::python::incref(boost::python::object(ss).ptr());
        }
        return boost::python::incref(boost::python::object().ptr());
    }
};

struct make_python_ppst_binary {
    static PyObject* convert(ppst_binary const &s) {
        if (s.data) {
            string ss;
            ss = string(s.data, s.size);
            free(s.data);
            return boost::python::incref(boost::python::object(ss).ptr());
        }
        return boost::python::incref(boost::python::object().ptr());
    }
};

struct make_python_pst_item_email {
    static PyObject* convert(pst_item_email* const &s) {
        if (s) return to_python_indirect<pst_item_email*, detail::make_reference_holder>()(s);
        return boost::python::incref(boost::python::object().ptr());
    }
};

struct make_python_pst_item_attach {
    static PyObject* convert(pst_item_attach* const &s) {
        if (s) return to_python_indirect<pst_item_attach*, detail::make_reference_holder>()(s);
        return boost::python::incref(boost::python::object().ptr());
    }
};

struct make_python_pst_desc_tree {
    static PyObject* convert(pst_desc_tree* const &s) {
        if (s) return to_python_indirect<pst_desc_tree*, detail::make_reference_holder>()(s);
        return boost::python::incref(boost::python::object().ptr());
    }
};

struct make_python_pst_index_ll {
    static PyObject* convert(pst_index_ll* const &s) {
        if (s) return to_python_indirect<pst_index_ll*, detail::make_reference_holder>()(s);
        return boost::python::incref(boost::python::object().ptr());
    }
};

struct make_python_FILE {
    static PyObject* convert(FILE* const &s) {
        if (s) return to_python_indirect<FILE*, detail::make_reference_holder>()(s);
        return boost::python::incref(boost::python::object().ptr());
    }
};

BOOST_PYTHON_MODULE(_libpst)
{
    //boost::python::docstring_options doc_options();

    to_python_converter<pst_binary,       make_python_pst_binary>();
    to_python_converter<ppst_binary,      make_python_ppst_binary>();
    to_python_converter<char*,            make_python_string>();
    to_python_converter<pst_item_email*,  make_python_pst_item_email>();
    to_python_converter<pst_item_attach*, make_python_pst_item_attach>();
    to_python_converter<pst_desc_tree*,   make_python_pst_desc_tree>();
    to_python_converter<pst_index_ll*,    make_python_pst_index_ll>();
    to_python_converter<FILE*,            make_python_FILE>();

    class_<FILE>("FILE")
        ;

    class_<FILETIME>("FILETIME")
        .def_readwrite("dwLowDateTime",         &FILETIME::dwLowDateTime)
        .def_readwrite("dwHighDateTime",        &FILETIME::dwHighDateTime)
        ;

    class_<pst_entryid>("pst_entryid")
        .def_readonly("u1",                     &pst_entryid::u1)
        .def_readonly("entryid",                &pst_entryid::entryid)
        .def_readonly("id",                     &pst_entryid::id)
        ;

    class_<pst_index_ll>("pst_index_ll")
        .def_readonly("i_id",                   &pst_index_ll::i_id)
        .def_readonly("offset",                 &pst_index_ll::offset)
        .def_readonly("size",                   &pst_index_ll::size)
        .def_readonly("u1",                     &pst_index_ll::u1)
        .add_property("next",                   make_getter(&pst_index_ll::next, return_value_policy<reference_existing_object>()))
        ;

    class_<pst_id2_tree>("pst_id2_tree")
        .def_readonly("id2",                    &pst_id2_tree::id2)
        .add_property("id",                     make_getter(&pst_id2_tree::id,    return_value_policy<reference_existing_object>()))
        .add_property("child",                  make_getter(&pst_id2_tree::child, return_value_policy<reference_existing_object>()))
        .add_property("next",                   make_getter(&pst_id2_tree::next,  return_value_policy<reference_existing_object>()))
        ;

    class_<pst_desc_tree>("pst_desc_tree")
        .def_readonly("d_id",                   &pst_desc_tree::d_id)
        .def_readonly("parent_d_id",            &pst_desc_tree::parent_d_id)
        .add_property("desc",                   make_getter(&pst_desc_tree::desc,       return_value_policy<reference_existing_object>()))
        .add_property("assoc_tree",             make_getter(&pst_desc_tree::assoc_tree, return_value_policy<reference_existing_object>()))
        .def_readonly("no_child",               &pst_desc_tree::no_child)
        .add_property("prev",                   make_getter(&pst_desc_tree::prev,       return_value_policy<reference_existing_object>()))
        .add_property("next",                   make_getter(&pst_desc_tree::next,       return_value_policy<reference_existing_object>()))
        .add_property("parent",                 make_getter(&pst_desc_tree::parent,     return_value_policy<reference_existing_object>()))
        .add_property("child",                  make_getter(&pst_desc_tree::child,      return_value_policy<reference_existing_object>()))
        .add_property("child_tail",             make_getter(&pst_desc_tree::child_tail, return_value_policy<reference_existing_object>()))
        ;

    class_<pst_string>("pst_string")
        .def_readonly("is_utf8",                &pst_string::is_utf8)
        .def_readonly("str",                    &pst_string::str)
        ;

    class_<pst_item_email>("pst_item_email")
        .add_property("arrival_date",           make_getter(&pst_item_email::arrival_date, return_value_policy<reference_existing_object>()))
        .def_readonly("autoforward",            &pst_item_email::autoforward)
        .def_readonly("cc_address",             &pst_item_email::cc_address)
        .def_readonly("bcc_address",            &pst_item_email::bcc_address)
        .add_property("conversation_index",     make_getter(&pst_item_email::conversation_index, return_value_policy<return_by_value>()))
        .def_readonly("conversion_prohibited",  &pst_item_email::conversion_prohibited)
        .def_readonly("delete_after_submit",    &pst_item_email::delete_after_submit)
        .def_readonly("delivery_report",        &pst_item_email::delivery_report)
        .add_property("encrypted_body",         make_getter(&pst_item_email::encrypted_body, return_value_policy<return_by_value>()))
        .add_property("encrypted_htmlbody",     make_getter(&pst_item_email::encrypted_htmlbody, return_value_policy<return_by_value>()))
        .def_readonly("header",                 &pst_item_email::header)
        .def_readonly("htmlbody",               &pst_item_email::htmlbody)
        .def_readonly("importance",             &pst_item_email::importance)
        .def_readonly("in_reply_to",            &pst_item_email::in_reply_to)
        .def_readonly("message_cc_me",          &pst_item_email::message_cc_me)
        .def_readonly("message_recip_me",       &pst_item_email::message_recip_me)
        .def_readonly("message_to_me",          &pst_item_email::message_to_me)
        .def_readonly("messageid",              &pst_item_email::messageid)
        .def_readonly("original_sensitivity",   &pst_item_email::original_sensitivity)
        .def_readonly("original_bcc",           &pst_item_email::original_bcc)
        .def_readonly("original_cc",            &pst_item_email::original_cc)
        .def_readonly("original_to",            &pst_item_email::original_to)
        .def_readonly("outlook_recipient",      &pst_item_email::outlook_recipient)
        .def_readonly("outlook_recipient_name", &pst_item_email::outlook_recipient_name)
        .def_readonly("outlook_recipient2",     &pst_item_email::outlook_recipient2)
        .def_readonly("outlook_sender",         &pst_item_email::outlook_sender)
        .def_readonly("outlook_sender_name",    &pst_item_email::outlook_sender_name)
        .def_readonly("outlook_sender2",        &pst_item_email::outlook_sender2)
        .def_readonly("priority",               &pst_item_email::priority)
        .def_readonly("processed_subject",      &pst_item_email::processed_subject)
        .def_readonly("read_receipt",           &pst_item_email::read_receipt)
        .def_readonly("recip_access",           &pst_item_email::recip_access)
        .def_readonly("recip_address",          &pst_item_email::recip_address)
        .def_readonly("recip2_access",          &pst_item_email::recip2_access)
        .def_readonly("recip2_address",         &pst_item_email::recip2_address)
        .def_readonly("reply_requested",        &pst_item_email::reply_requested)
        .def_readonly("reply_to",               &pst_item_email::reply_to)
        .def_readonly("return_path_address",    &pst_item_email::return_path_address)
        .def_readonly("rtf_body_char_count",    &pst_item_email::rtf_body_char_count)
        .def_readonly("rtf_body_crc",           &pst_item_email::rtf_body_crc)
        .def_readonly("rtf_body_tag",           &pst_item_email::rtf_body_tag)
        .add_property("rtf_compressed",         make_getter(&pst_item_email::rtf_compressed, return_value_policy<return_by_value>()))
        .def_readonly("rtf_in_sync",            &pst_item_email::rtf_in_sync)
        .def_readonly("rtf_ws_prefix_count",    &pst_item_email::rtf_ws_prefix_count)
        .def_readonly("rtf_ws_trailing_count",  &pst_item_email::rtf_ws_trailing_count)
        .def_readonly("sender_access",          &pst_item_email::sender_access)
        .def_readonly("sender_address",         &pst_item_email::sender_address)
        .def_readonly("sender2_access",         &pst_item_email::sender2_access)
        .def_readonly("sender2_address",        &pst_item_email::sender2_address)
        .def_readonly("sensitivity",            &pst_item_email::sensitivity)
        .add_property("sent_date",              make_getter(&pst_item_email::sent_date, return_value_policy<reference_existing_object>()))
        .add_property("sentmail_folder",        make_getter(&pst_item_email::sentmail_folder, return_value_policy<reference_existing_object>()))
        .def_readonly("sentto_address",         &pst_item_email::sentto_address)
        .def_readonly("report_text",            &pst_item_email::report_text)
        .add_property("report_time",            make_getter(&pst_item_email::report_time, return_value_policy<reference_existing_object>()))
        .def_readonly("ndr_reason_code",        &pst_item_email::ndr_reason_code)
        .def_readonly("ndr_diag_code",          &pst_item_email::ndr_diag_code)
        .def_readonly("supplementary_info",     &pst_item_email::supplementary_info)
        .def_readonly("ndr_status_code",        &pst_item_email::ndr_status_code)
        ;

    class_<pst_item_folder>("pst_item_folder")
        .def_readonly("item_count",             &pst_item_folder::item_count)
        .def_readonly("unseen_item_count",      &pst_item_folder::unseen_item_count)
        .def_readonly("assoc_count",            &pst_item_folder::assoc_count)
        .def_readonly("subfolder",              &pst_item_folder::subfolder)
        ;

    class_<pst_item_message_store>("pst_item_message_store")
        .add_property("top_of_personal_folder", make_getter(&pst_item_message_store::top_of_personal_folder, return_value_policy<reference_existing_object>()))
        .add_property("default_outbox_folder",  make_getter(&pst_item_message_store::default_outbox_folder, return_value_policy<reference_existing_object>()))
        .add_property("deleted_items_folder",   make_getter(&pst_item_message_store::deleted_items_folder, return_value_policy<reference_existing_object>()))
        .add_property("sent_items_folder",      make_getter(&pst_item_message_store::sent_items_folder, return_value_policy<reference_existing_object>()))
        .add_property("user_views_folder",      make_getter(&pst_item_message_store::user_views_folder, return_value_policy<reference_existing_object>()))
        .add_property("common_view_folder",     make_getter(&pst_item_message_store::common_view_folder, return_value_policy<reference_existing_object>()))
        .add_property("search_root_folder",     make_getter(&pst_item_message_store::search_root_folder, return_value_policy<reference_existing_object>()))
        .add_property("top_of_folder",          make_getter(&pst_item_message_store::top_of_folder, return_value_policy<reference_existing_object>()))
        .def_readonly("valid_mask",             &pst_item_message_store::valid_mask)
        .def_readonly("pwd_chksum",             &pst_item_message_store::pwd_chksum)
        ;

    class_<pst_item_contact>("pst_item_contact")
        .def_readonly("account_name",           &pst_item_contact::account_name)
        .def_readonly("address1",               &pst_item_contact::address1)
        .def_readonly("address1a",              &pst_item_contact::address1a)
        .def_readonly("address1_desc",          &pst_item_contact::address1_desc)
        .def_readonly("address1_transport",     &pst_item_contact::address1_transport)
        .def_readonly("address2",               &pst_item_contact::address2)
        .def_readonly("address2a",              &pst_item_contact::address2a)
        .def_readonly("address2_desc",          &pst_item_contact::address2_desc)
        .def_readonly("address2_transport",     &pst_item_contact::address2_transport)
        .def_readonly("address3",               &pst_item_contact::address3)
        .def_readonly("address3a",              &pst_item_contact::address3a)
        .def_readonly("address3_desc",          &pst_item_contact::address3_desc)
        .def_readonly("address3_transport",     &pst_item_contact::address3_transport)
        .def_readonly("assistant_name",         &pst_item_contact::assistant_name)
        .def_readonly("assistant_phone",        &pst_item_contact::assistant_phone)
        .def_readonly("billing_information",    &pst_item_contact::billing_information)
        .add_property("birthday",               make_getter(&pst_item_contact::birthday, return_value_policy<reference_existing_object>()))
        .def_readonly("business_address",       &pst_item_contact::business_address)
        .def_readonly("business_city",          &pst_item_contact::business_city)
        .def_readonly("business_country",       &pst_item_contact::business_country)
        .def_readonly("business_fax",           &pst_item_contact::business_fax)
        .def_readonly("business_homepage",      &pst_item_contact::business_homepage)
        .def_readonly("business_phone",         &pst_item_contact::business_phone)
        .def_readonly("business_phone2",        &pst_item_contact::business_phone2)
        .def_readonly("business_po_box",        &pst_item_contact::business_po_box)
        .def_readonly("business_postal_code",   &pst_item_contact::business_postal_code)
        .def_readonly("business_state",         &pst_item_contact::business_state)
        .def_readonly("business_street",        &pst_item_contact::business_street)
        .def_readonly("callback_phone",         &pst_item_contact::callback_phone)
        .def_readonly("car_phone",              &pst_item_contact::car_phone)
        .def_readonly("company_main_phone",     &pst_item_contact::company_main_phone)
        .def_readonly("company_name",           &pst_item_contact::company_name)
        .def_readonly("computer_name",          &pst_item_contact::computer_name)
        .def_readonly("customer_id",            &pst_item_contact::customer_id)
        .def_readonly("def_postal_address",     &pst_item_contact::def_postal_address)
        .def_readonly("department",             &pst_item_contact::department)
        .def_readonly("display_name_prefix",    &pst_item_contact::display_name_prefix)
        .def_readonly("first_name",             &pst_item_contact::first_name)
        .def_readonly("followup",               &pst_item_contact::followup)
        .def_readonly("free_busy_address",      &pst_item_contact::free_busy_address)
        .def_readonly("ftp_site",               &pst_item_contact::ftp_site)
        .def_readonly("fullname",               &pst_item_contact::fullname)
        .def_readonly("gender",                 &pst_item_contact::gender)
        .def_readonly("gov_id",                 &pst_item_contact::gov_id)
        .def_readonly("hobbies",                &pst_item_contact::hobbies)
        .def_readonly("home_address",           &pst_item_contact::home_address)
        .def_readonly("home_city",              &pst_item_contact::home_city)
        .def_readonly("home_country",           &pst_item_contact::home_country)
        .def_readonly("home_fax",               &pst_item_contact::home_fax)
        .def_readonly("home_phone",             &pst_item_contact::home_phone)
        .def_readonly("home_phone2",            &pst_item_contact::home_phone2)
        .def_readonly("home_po_box",            &pst_item_contact::home_po_box)
        .def_readonly("home_postal_code",       &pst_item_contact::home_postal_code)
        .def_readonly("home_state",             &pst_item_contact::home_state)
        .def_readonly("home_street",            &pst_item_contact::home_street)
        .def_readonly("initials",               &pst_item_contact::initials)
        .def_readonly("isdn_phone",             &pst_item_contact::isdn_phone)
        .def_readonly("job_title",              &pst_item_contact::job_title)
        .def_readonly("keyword",                &pst_item_contact::keyword)
        .def_readonly("language",               &pst_item_contact::language)
        .def_readonly("location",               &pst_item_contact::location)
        .def_readonly("mail_permission",        &pst_item_contact::mail_permission)
        .def_readonly("manager_name",           &pst_item_contact::manager_name)
        .def_readonly("middle_name",            &pst_item_contact::middle_name)
        .def_readonly("mileage",                &pst_item_contact::mileage)
        .def_readonly("mobile_phone",           &pst_item_contact::mobile_phone)
        .def_readonly("nickname",               &pst_item_contact::nickname)
        .def_readonly("office_loc",             &pst_item_contact::office_loc)
        .def_readonly("common_name",            &pst_item_contact::common_name)
        .def_readonly("org_id",                 &pst_item_contact::org_id)
        .def_readonly("other_address",          &pst_item_contact::other_address)
        .def_readonly("other_city",             &pst_item_contact::other_city)
        .def_readonly("other_country",          &pst_item_contact::other_country)
        .def_readonly("other_phone",            &pst_item_contact::other_phone)
        .def_readonly("other_po_box",           &pst_item_contact::other_po_box)
        .def_readonly("other_postal_code",      &pst_item_contact::other_postal_code)
        .def_readonly("other_state",            &pst_item_contact::other_state)
        .def_readonly("other_street",           &pst_item_contact::other_street)
        .def_readonly("pager_phone",            &pst_item_contact::pager_phone)
        .def_readonly("personal_homepage",      &pst_item_contact::personal_homepage)
        .def_readonly("pref_name",              &pst_item_contact::pref_name)
        .def_readonly("primary_fax",            &pst_item_contact::primary_fax)
        .def_readonly("primary_phone",          &pst_item_contact::primary_phone)
        .def_readonly("profession",             &pst_item_contact::profession)
        .def_readonly("radio_phone",            &pst_item_contact::radio_phone)
        .def_readonly("rich_text",              &pst_item_contact::rich_text)
        .def_readonly("spouse_name",            &pst_item_contact::spouse_name)
        .def_readonly("suffix",                 &pst_item_contact::suffix)
        .def_readonly("surname",                &pst_item_contact::surname)
        .def_readonly("telex",                  &pst_item_contact::telex)
        .def_readonly("transmittable_display_name", &pst_item_contact::transmittable_display_name)
        .def_readonly("ttytdd_phone",               &pst_item_contact::ttytdd_phone)
        .add_property("wedding_anniversary",        make_getter(&pst_item_contact::wedding_anniversary, return_value_policy<reference_existing_object>()))
        .def_readonly("work_address_street",        &pst_item_contact::work_address_street)
        .def_readonly("work_address_city",          &pst_item_contact::work_address_city)
        .def_readonly("work_address_state",         &pst_item_contact::work_address_state)
        .def_readonly("work_address_postalcode",    &pst_item_contact::work_address_postalcode)
        .def_readonly("work_address_country",       &pst_item_contact::work_address_country)
        .def_readonly("work_address_postofficebox", &pst_item_contact::work_address_postofficebox)
        ;

    class_<pst_item_attach>("pst_item_attach")
        .def_readonly("filename1",              &pst_item_attach::filename1)
        .def_readonly("filename2",              &pst_item_attach::filename2)
        .def_readonly("mimetype",               &pst_item_attach::mimetype)
        .add_property("data",                   make_getter(&pst_item_attach::data, return_value_policy<return_by_value>()))
        .def_readonly("id2_val",                &pst_item_attach::id2_val)
        .def_readonly("i_id",                   &pst_item_attach::i_id)
        .add_property("id2_head",               make_getter(&pst_item_attach::id2_head, return_value_policy<reference_existing_object>()))
        .def_readonly("method",                 &pst_item_attach::method)
        .def_readonly("position",               &pst_item_attach::position)
        .def_readonly("sequence",               &pst_item_attach::sequence)
        .add_property("next",                   make_getter(&pst_item_attach::next, return_value_policy<reference_existing_object>()))
        ;

    class_<pst_item_extra_field>("pst_item_extra_field")
        .def_readonly("field_name",             &pst_item_extra_field::field_name)
        .def_readonly("value",                  &pst_item_extra_field::value)
        .add_property("next",                   make_getter(&pst_item_extra_field::next, return_value_policy<reference_existing_object>()))
        ;

    class_<pst_item_journal>("pst_item_journal")
        .add_property("start",                  make_getter(&pst_item_journal::start, return_value_policy<reference_existing_object>()))
        .add_property("end",                    make_getter(&pst_item_journal::end, return_value_policy<reference_existing_object>()))
        .def_readonly("type",                   &pst_item_journal::type)
        .def_readonly("description",            &pst_item_journal::description)
        ;

    class_<pst_recurrence>("pst_recurrence")
        .def_readonly("signature",              &pst_recurrence::signature)
        .def_readonly("type",                   &pst_recurrence::type)
        .def_readonly("sub_type",               &pst_recurrence::sub_type)
        .def_readonly("parm1",                  &pst_recurrence::parm1)
        .def_readonly("parm2",                  &pst_recurrence::parm2)
        .def_readonly("parm3",                  &pst_recurrence::parm3)
        .def_readonly("parm4",                  &pst_recurrence::parm4)
        .def_readonly("parm5",                  &pst_recurrence::parm5)
        .def_readonly("termination",            &pst_recurrence::termination)
        .def_readonly("interval",               &pst_recurrence::interval)
        .def_readonly("bydaymask",              &pst_recurrence::bydaymask)
        .def_readonly("dayofmonth",             &pst_recurrence::dayofmonth)
        .def_readonly("monthofyear",            &pst_recurrence::monthofyear)
        .def_readonly("position",               &pst_recurrence::position)
        .def_readonly("count",                  &pst_recurrence::count)
        ;

    class_<pst_item_appointment>("pst_item_appointment")
        .add_property("start",                  make_getter(&pst_item_appointment::start, return_value_policy<reference_existing_object>()))
        .add_property("end",                    make_getter(&pst_item_appointment::end, return_value_policy<reference_existing_object>()))
        .def_readonly("location",               &pst_item_appointment::location)
        .def_readonly("alarm",                  &pst_item_appointment::alarm)
        .add_property("reminder",               make_getter(&pst_item_appointment::reminder, return_value_policy<reference_existing_object>()))
        .def_readonly("alarm_minutes",          &pst_item_appointment::alarm_minutes)
        .def_readonly("alarm_filename",         &pst_item_appointment::alarm_filename)
        .def_readonly("timezonestring",         &pst_item_appointment::timezonestring)
        .def_readonly("showas",                 &pst_item_appointment::showas)
        .def_readonly("label",                  &pst_item_appointment::label)
        .def_readonly("all_day",                &pst_item_appointment::all_day)
        .def_readonly("is_recurring",           &pst_item_appointment::is_recurring)
        .def_readonly("recurrence_type",        &pst_item_appointment::recurrence_type)
        .def_readonly("recurrence_description", &pst_item_appointment::recurrence_description)
        .add_property("recurrence_data",        make_getter(&pst_item_appointment::recurrence_data, return_value_policy<return_by_value>()))
        .add_property("recurrence_start",       make_getter(&pst_item_appointment::recurrence_start, return_value_policy<reference_existing_object>()))
        .add_property("recurrence_end",         make_getter(&pst_item_appointment::recurrence_end, return_value_policy<reference_existing_object>()))
        ;

    class_<pst_item>("pst_item")
        .add_property("email",                  make_getter(&pst_item::email,         return_value_policy<reference_existing_object>()))
        .add_property("folder",                 make_getter(&pst_item::folder,        return_value_policy<reference_existing_object>()))
        .add_property("contact",                make_getter(&pst_item::contact,       return_value_policy<reference_existing_object>()))
        .add_property("attach",                 make_getter(&pst_item::attach,        return_value_policy<reference_existing_object>()))
        .add_property("message_store",          make_getter(&pst_item::message_store, return_value_policy<reference_existing_object>()))
        .add_property("extra_fields",           make_getter(&pst_item::extra_fields,  return_value_policy<reference_existing_object>()))
        .add_property("journal",                make_getter(&pst_item::journal,       return_value_policy<reference_existing_object>()))
        .add_property("appointment",            make_getter(&pst_item::appointment,   return_value_policy<reference_existing_object>()))
        .def_readonly("block_id",               &pst_item::block_id)
        .def_readonly("type",                   &pst_item::type)
        .def_readonly("ascii_type",             &pst_item::ascii_type)
        .def_readonly("flags",                  &pst_item::flags)
        .def_readonly("file_as",                &pst_item::file_as)
        .def_readonly("comment",                &pst_item::comment)
        .def_readonly("body_charset",           &pst_item::body_charset)
        .def_readonly("body",                   &pst_item::body)
        .def_readonly("subject",                &pst_item::subject)
        .def_readonly("internet_cpid",          &pst_item::internet_cpid)
        .def_readonly("message_codepage",       &pst_item::message_codepage)
        .def_readonly("message_size",           &pst_item::message_size)
        .def_readonly("outlook_version",        &pst_item::outlook_version)
        .add_property("record_key",             make_getter(&pst_item::record_key, return_value_policy<return_by_value>()))
        .add_property("predecessor_change",     make_getter(&pst_item::predecessor_change, return_value_policy<return_by_value>()))
        .def_readonly("response_requested",     &pst_item::response_requested)
        .add_property("create_date",            make_getter(&pst_item::create_date, return_value_policy<reference_existing_object>()))
        .add_property("modify_date",            make_getter(&pst_item::modify_date, return_value_policy<reference_existing_object>()))
        .def_readonly("private_member",         &pst_item::private_member)
        ;

    class_<pst_x_attrib_ll>("pst_x_attrib_ll")
        .def_readonly("mytype",                 &pst_x_attrib_ll::mytype)
        .def_readonly("map",                    &pst_x_attrib_ll::map)
        .def_readonly("data",                   &pst_x_attrib_ll::data)
        .add_property("next",                   make_getter(&pst_x_attrib_ll::next, return_value_policy<reference_existing_object>()))
        ;

    class_<pst_file>("pst_file")
        .def_readonly("cwd",                    &pst_file::cwd)
        .def_readonly("fname",                  &pst_file::fname)
        .add_property("i_head",                 make_getter(&pst_file::i_head, return_value_policy<reference_existing_object>()))
        .add_property("i_tail",                 make_getter(&pst_file::i_tail, return_value_policy<reference_existing_object>()))
        .add_property("d_head",                 make_getter(&pst_file::d_head, return_value_policy<reference_existing_object>()))
        .add_property("d_tail",                 make_getter(&pst_file::d_tail, return_value_policy<reference_existing_object>()))
        .add_property("x_head",                 make_getter(&pst_file::x_head, return_value_policy<reference_existing_object>()))
        .def_readonly("do_read64",              &pst_file::do_read64)
        .def_readonly("index1",                 &pst_file::index1)
        .def_readonly("index1_back",            &pst_file::index1_back)
        .def_readonly("index2",                 &pst_file::index2)
        .def_readonly("index2_back",            &pst_file::index2_back)
        .def_readonly("size",                   &pst_file::size)
        .def_readonly("encryption",             &pst_file::encryption)
        .def_readonly("ind_type",               &pst_file::ind_type)
        ;

    class_<pst>("pst", init<string,string>())
        .def("pst_getTopOfFolders",             &pst::pst_getTopOfFolders, return_value_policy<reference_existing_object>())
        .def("pst_attach_to_mem",               &pst::pst_attach_to_mem)
        .def("pst_attach_to_file",              &pst::pst_attach_to_file)
        .def("pst_attach_to_file_base64",       &pst::pst_attach_to_file_base64)
        .def("pst_getNextDptr",                 &pst::pst_getNextDptr,     return_value_policy<reference_existing_object>())
        .def("pst_parse_item",                  &pst::pst_parse_item,      return_value_policy<reference_existing_object>())
        .def("pst_freeItem",                    &pst::pst_freeItem)
        .def("pst_getID",                       &pst::pst_getID,           return_value_policy<reference_existing_object>())
        .def("pst_ff_getIDblock_dec",           &pst::pst_ff_getIDblock_dec)
        .def("pst_rfc2426_escape",              &pst::pst_rfc2426_escape)
        .def("pst_rfc2425_datetime_format",     &pst::pst_rfc2425_datetime_format)
        .def("pst_rfc2445_datetime_format",     &pst::pst_rfc2445_datetime_format)
        .def("pst_default_charset",             &pst::pst_default_charset)
        .def("pst_convert_utf8_null",           &pst::pst_convert_utf8_null)
        .def("pst_convert_utf8",                &pst::pst_convert_utf8)
        .def("ppst_open_file",                  &pst::ppst_open_file,      return_value_policy<reference_existing_object>())
        .def("ppst_close_file",                 &pst::ppst_close_file)
        ;
}


