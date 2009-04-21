#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#include <iostream>
#include <boost/python.hpp>
#include <iostream>

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
                   pst(const string filename);
    virtual        ~pst();
    pst_desc_tree* pst_getTopOfFolders();
    ppst_binary    pst_attach_to_mem(pst_item_attach *attach);
    size_t         pst_attach_to_file(pst_item_attach *attach, FILE* fp);
    size_t         pst_attach_to_file_base64(pst_item_attach *attach, FILE* fp);
    pst_desc_tree* pst_getNextDptr(pst_desc_tree* d);
    pst_item*      pst_parse_item(pst_desc_tree *d_ptr, pst_id2_tree *m_head);
    void           pst_freeItem(pst_item *item);
    pst_index_ll*  pst_getID(uint64_t i_id);
    int            pst_decrypt(uint64_t i_id, char *buf, size_t size, unsigned char type);
    size_t         pst_ff_getIDblock_dec(uint64_t i_id, char **buf);
    size_t         pst_ff_getIDblock(uint64_t i_id, char** buf);
    string         pst_rfc2426_escape(char *str);
    string         pst_rfc2425_datetime_format(const FILETIME *ft);
    string         pst_rfc2445_datetime_format(const FILETIME *ft);
    string         pst_default_charset(pst_item *item);
    void           pst_convert_utf8_null(pst_item *item, pst_string *str);
    void           pst_convert_utf8(pst_item *item, pst_string *str);

private:
    bool            is_open;
    pst_file        pf;
    pst_item*       root;
    pst_desc_tree*  topf;

};


pst::pst(const string filename) {
    char *f = (char *)filename.c_str(); // ok, since pst_open does not actually modify this buffer, and newer versions will change the signature to const anyway
    is_open = (::pst_open(&pf, f) == 0);
    root = NULL;
    topf = NULL;
    if (is_open) {
        ::pst_load_index(&pf);
        ::pst_load_extended_attributes(&pf);
        root = ::pst_parse_item(&pf, pf.d_head, NULL);
        topf = ::pst_getTopOfFolders(&pf, root)->child;
    }
}

pst::~pst() {
    if (root) pst_freeItem(root);
    if (is_open) ::pst_close(&pf);
}

pst_desc_tree* pst::pst_getTopOfFolders() {
    return topf;
}

ppst_binary    pst::pst_attach_to_mem(pst_item_attach *attach) {
    ppst_binary rc;
    rc.size = rc.size = ::pst_attach_to_mem(&pf, attach, &rc.data);
    return rc;
}

size_t         pst::pst_attach_to_file(pst_item_attach *attach, FILE* fp) {
    return ::pst_attach_to_file(&pf, attach, fp);
}

size_t         pst::pst_attach_to_file_base64(pst_item_attach *attach, FILE* fp) {
    return ::pst_attach_to_file_base64(&pf, attach, fp);
}

pst_desc_tree* pst::pst_getNextDptr(pst_desc_tree* d) {
    return ::pst_getNextDptr(d);
}

pst_item*      pst::pst_parse_item (pst_desc_tree *d_ptr, pst_id2_tree *m_head) {
    return ::pst_parse_item(&pf, d_ptr, m_head);
}

void           pst::pst_freeItem(pst_item *item) {
    return ::pst_freeItem(item);
}

pst_index_ll*  pst::pst_getID(uint64_t i_id) {
    return ::pst_getID(&pf, i_id);
}

int            pst::pst_decrypt(uint64_t i_id, char *buf, size_t size, unsigned char type) {
    return ::pst_decrypt(i_id, buf, size, type);
}

size_t         pst::pst_ff_getIDblock_dec(uint64_t i_id, char **buf) {
    return ::pst_ff_getIDblock_dec(&pf, i_id, buf);
}

size_t         pst::pst_ff_getIDblock(uint64_t i_id, char** buf) {
    return ::pst_ff_getIDblock(&pf, i_id, buf);
}

string         pst::pst_rfc2426_escape(char *str) {
    return ::pst_rfc2426_escape(str);
}

string         pst::pst_rfc2425_datetime_format(const FILETIME *ft) {
    return ::pst_rfc2425_datetime_format((FILETIME *)ft);   // cast away const is ok, since libpst did not modify it anyway, and the signature will change in more recent versions
}

string         pst::pst_rfc2445_datetime_format(const FILETIME *ft) {
    return ::pst_rfc2445_datetime_format((FILETIME *)ft);   // cast away const is ok, since libpst did not modify it anyway, and the signature will change in more recent versions
}

string         pst::pst_default_charset(pst_item *item) {
    return ::pst_default_charset(item);
}

void           pst::pst_convert_utf8_null(pst_item *item, pst_string *str) {
    ::pst_convert_utf8_null(item, str);
}

void           pst::pst_convert_utf8(pst_item *item, pst_string *str) {
    ::pst_convert_utf8(item, str);
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
        return NULL;
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
        return NULL;
    }
};

struct make_python_pst_item_email {
    static PyObject* convert(pst_item_email* const &s) {
        if (s) return to_python_indirect<pst_item_email*, detail::make_reference_holder>()(s);
        return NULL;
    }
};

struct make_python_pst_item_attach {
    static PyObject* convert(pst_item_attach* const &s) {
        if (s) return to_python_indirect<pst_item_attach*, detail::make_reference_holder>()(s);
        return NULL;
    }
};

struct make_python_pst_desc_tree {
    static PyObject* convert(pst_desc_tree* const &s) {
        if (s) return to_python_indirect<pst_desc_tree*, detail::make_reference_holder>()(s);
        return NULL;
    }
};

struct make_python_pst_index_ll {
    static PyObject* convert(pst_index_ll* const &s) {
        if (s) return to_python_indirect<pst_index_ll*, detail::make_reference_holder>()(s);
        return NULL;
    }
};

BOOST_PYTHON_MODULE(_libpst)
{
    to_python_converter<pst_binary,       make_python_pst_binary>();
    to_python_converter<ppst_binary,      make_python_ppst_binary>();
    to_python_converter<char*,            make_python_string>();
    to_python_converter<pst_item_email*,  make_python_pst_item_email>();
    to_python_converter<pst_item_attach*, make_python_pst_item_attach>();
    to_python_converter<pst_desc_tree*,   make_python_pst_desc_tree>();
    to_python_converter<pst_index_ll*,    make_python_pst_index_ll>();

    class_<FILETIME>("FILETIME")
        .def_readonly("dwLowDateTime",  &FILETIME::dwLowDateTime)
        .def_readonly("dwHighDateTime", &FILETIME::dwHighDateTime)
        ;

    class_<pst_entryid>("pst_entryid")
        .def_readonly("u1",      &pst_entryid::u1)
        .def_readonly("entryid", &pst_entryid::entryid)
        .def_readonly("id",      &pst_entryid::id)
        ;

    class_<pst_index_ll>("pst_index_ll")
        .def_readonly("i_id",               &pst_index_ll::i_id)
        .def_readonly("offset",             &pst_index_ll::offset)
        .def_readonly("size",               &pst_index_ll::size)
        .def_readonly("u1",                 &pst_index_ll::u1)
        .add_property("next",   make_getter(&pst_index_ll::next, return_value_policy<reference_existing_object>()))
        ;

    class_<pst_id2_tree>("pst_id2_tree")
        .def_readonly("id2",               &pst_id2_tree::id2)
        .add_property("id",    make_getter(&pst_id2_tree::id,    return_value_policy<reference_existing_object>()))
        .add_property("child", make_getter(&pst_id2_tree::child, return_value_policy<reference_existing_object>()))
        .add_property("next",  make_getter(&pst_id2_tree::next,  return_value_policy<reference_existing_object>()))
        ;

    class_<pst_desc_tree>("pst_desc_tree")
        .def_readonly("d_id",                    &pst_desc_tree::d_id)
        .def_readonly("parent_d_id",             &pst_desc_tree::parent_d_id)
        .add_property("desc",        make_getter(&pst_desc_tree::desc,       return_value_policy<reference_existing_object>()))
        .add_property("assoc_tree",  make_getter(&pst_desc_tree::assoc_tree, return_value_policy<reference_existing_object>()))
        .def_readonly("no_child",                &pst_desc_tree::no_child)
        .add_property("prev",        make_getter(&pst_desc_tree::prev,       return_value_policy<reference_existing_object>()))
        .add_property("next",        make_getter(&pst_desc_tree::next,       return_value_policy<reference_existing_object>()))
        .add_property("parent",      make_getter(&pst_desc_tree::parent,     return_value_policy<reference_existing_object>()))
        .add_property("child",       make_getter(&pst_desc_tree::child,      return_value_policy<reference_existing_object>()))
        .add_property("child_tail",  make_getter(&pst_desc_tree::child_tail, return_value_policy<reference_existing_object>()))
        ;

    class_<pst_string>("pst_string")
        .def_readonly("is_utf8", &pst_string::is_utf8)
        .def_readonly("str",     &pst_string::str)
        ;

    class_<pst_item_email>("pst_item_email")
        .add_property("arrival_date", make_getter(&pst_item_email::arrival_date, return_value_policy<reference_existing_object>()))
        .def_readonly("autoforward",              &pst_item_email::autoforward)
        .def_readonly("cc_address",               &pst_item_email::cc_address)
        .def_readonly("bcc_address",              &pst_item_email::bcc_address)
        .def_readonly("conversation_index",       &pst_item_email::conversation_index)
        .def_readonly("conversion_prohibited",    &pst_item_email::conversion_prohibited)
        .def_readonly("delete_after_submit",      &pst_item_email::delete_after_submit)
        .def_readonly("delivery_report",          &pst_item_email::delivery_report)
        .def_readonly("encrypted_body",           &pst_item_email::encrypted_body)
        .def_readonly("encrypted_htmlbody",       &pst_item_email::encrypted_htmlbody)
        .def_readonly("header",                   &pst_item_email::header)
        .def_readonly("htmlbody",                 &pst_item_email::htmlbody)
        .def_readonly("importance",               &pst_item_email::importance)
        .def_readonly("in_reply_to",              &pst_item_email::in_reply_to)
        .def_readonly("message_cc_me",            &pst_item_email::message_cc_me)
        .def_readonly("message_recip_me",         &pst_item_email::message_recip_me)
        .def_readonly("message_to_me",            &pst_item_email::message_to_me)
        .def_readonly("messageid",                &pst_item_email::messageid)
        .def_readonly("original_sensitivity",     &pst_item_email::original_sensitivity)
        .def_readonly("original_bcc",             &pst_item_email::original_bcc)
        .def_readonly("original_cc",              &pst_item_email::original_cc)
        .def_readonly("original_to",              &pst_item_email::original_to)
        ;

    class_<pst_item_folder>("pst_item_folder")
        .def_readonly("item_count", &pst_item_folder::item_count)
        ;

    class_<pst_item_message_store>("pst_item_message_store")
        .add_property("top_of_personal_folder", make_getter(&pst_item_message_store::top_of_personal_folder, return_value_policy<reference_existing_object>()))
        ;

    class_<pst_item_contact>("pst_item_contact")
        .def_readonly("account_name", &pst_item_contact::account_name)
        ;

    class_<pst_item_attach>("pst_item_attach")
        .def_readonly("filename1",             &pst_item_attach::filename1)
        .def_readonly("filename2",             &pst_item_attach::filename2)
        .def_readonly("mimetype",              &pst_item_attach::mimetype)
        .def_readonly("data",                  &pst_item_attach::data)
        .def_readonly("id2_val",               &pst_item_attach::id2_val)
        .def_readonly("i_id",                  &pst_item_attach::i_id)
        .add_property("id2_head",  make_getter(&pst_item_attach::id2_head, return_value_policy<reference_existing_object>()))
        .def_readonly("method",                &pst_item_attach::method)
        .def_readonly("position",              &pst_item_attach::position)
        .def_readonly("sequence",              &pst_item_attach::sequence)
        .add_property("next",      make_getter(&pst_item_attach::next, return_value_policy<reference_existing_object>()))
        ;

    class_<pst_item_extra_field>("pst_item_extra_field")
        .def_readonly("field_name", &pst_item_extra_field::field_name)
        ;

    class_<pst_item_journal>("pst_item_journal")
        .def_readonly("description", &pst_item_journal::description)
        ;

    class_<pst_item_appointment>("pst_item_appointment")
        .add_property("end",   make_getter(&pst_item_appointment::end, return_value_policy<reference_existing_object>()))
        .def_readonly("label", &pst_item_appointment::label)
        ;

    class_<pst_item>("pst_item")
        .add_property("email",           make_getter(&pst_item::email,         return_value_policy<reference_existing_object>()))
        .add_property("folder",          make_getter(&pst_item::folder,        return_value_policy<reference_existing_object>()))
        .add_property("contact",         make_getter(&pst_item::contact,       return_value_policy<reference_existing_object>()))
        .add_property("attach",          make_getter(&pst_item::attach,        return_value_policy<reference_existing_object>()))
        .add_property("message_store",   make_getter(&pst_item::message_store, return_value_policy<reference_existing_object>()))
        .add_property("extra_fields",    make_getter(&pst_item::extra_fields,  return_value_policy<reference_existing_object>()))
        .add_property("journal",         make_getter(&pst_item::journal,       return_value_policy<reference_existing_object>()))
        .add_property("appointment",     make_getter(&pst_item::appointment,   return_value_policy<reference_existing_object>()))
        .def_readonly("type",                        &pst_item::type)
        .def_readonly("ascii_type",                  &pst_item::ascii_type)
        .def_readonly("flags",                       &pst_item::flags)
        .def_readonly("file_as",                     &pst_item::file_as)
        .def_readonly("comment",                     &pst_item::comment)
        .def_readonly("body_charset",                &pst_item::body_charset)
        .def_readonly("body",                        &pst_item::body)
        .def_readonly("subject",                     &pst_item::subject)
        .def_readonly("internet_cpid",               &pst_item::internet_cpid)
        .def_readonly("message_codepage",            &pst_item::message_codepage)
        .def_readonly("message_size",                &pst_item::message_size)
        .def_readonly("outlook_version",             &pst_item::outlook_version)
        .def_readonly("record_key",                  &pst_item::record_key)
        .def_readonly("predecessor_change",          &pst_item::predecessor_change)
        .def_readonly("response_requested",          &pst_item::response_requested)
        .add_property("create_date",     make_getter(&pst_item::create_date, return_value_policy<reference_existing_object>()))
        .add_property("modify_date",     make_getter(&pst_item::modify_date, return_value_policy<reference_existing_object>()))
        .def_readonly("private_member",              &pst_item::private_member)
        ;

    class_<pst_x_attrib_ll>("pst_x_attrib_ll")
        .def_readonly("mytype", &pst_x_attrib_ll::mytype)
        .def_readonly("map",    &pst_x_attrib_ll::map)
        .def_readonly("data",   &pst_x_attrib_ll::data)
        .def_readonly("next",   &pst_x_attrib_ll::next)
        ;

    class_<pst_file>("pst_file")
        .add_property("i_head",      make_getter(&pst_file::i_head, return_value_policy<reference_existing_object>()))
        .add_property("i_tail",      make_getter(&pst_file::i_tail, return_value_policy<reference_existing_object>()))
        .add_property("d_head",      make_getter(&pst_file::d_head, return_value_policy<reference_existing_object>()))
        .add_property("d_tail",      make_getter(&pst_file::d_tail, return_value_policy<reference_existing_object>()))
        .def_readonly("do_read64",   &pst_file::do_read64)
        .def_readonly("index1",      &pst_file::index1)
        .def_readonly("index1_back", &pst_file::index1_back)
        .def_readonly("index2",      &pst_file::index2)
        .def_readonly("index2_back", &pst_file::index2_back)
        .def_readonly("size",        &pst_file::size)
        .def_readonly("encryption",  &pst_file::encryption)
        .def_readonly("ind_type",    &pst_file::ind_type)
        ;

    class_<pst>("pst", init<string>())
        .def("pst_getTopOfFolders",         &pst::pst_getTopOfFolders, return_value_policy<reference_existing_object>())
        .def("pst_attach_to_mem",           &pst::pst_attach_to_mem)
        .def("pst_attach_to_file",          &pst::pst_attach_to_file)
        .def("pst_attach_to_file_base64",   &pst::pst_attach_to_file_base64)
        .def("pst_getNextDptr",             &pst::pst_getNextDptr,     return_value_policy<reference_existing_object>())
        .def("pst_parse_item",              &pst::pst_parse_item,      return_value_policy<reference_existing_object>())
        .def("pst_freeItem",                &pst::pst_freeItem)
        .def("pst_getID",                   &pst::pst_getID,           return_value_policy<reference_existing_object>())
        .def("pst_decrypt",                 &pst::pst_decrypt)
        .def("pst_ff_getIDblock_dec",       &pst::pst_ff_getIDblock_dec)
        .def("pst_ff_getIDblock",           &pst::pst_ff_getIDblock)
        .def("pst_rfc2426_escape",          &pst::pst_rfc2426_escape)
        .def("pst_rfc2425_datetime_format", &pst::pst_rfc2425_datetime_format)
        .def("pst_rfc2445_datetime_format", &pst::pst_rfc2445_datetime_format)
        .def("pst_default_charset",         &pst::pst_default_charset)
        .def("pst_convert_utf8_null",       &pst::pst_convert_utf8_null)
        .def("pst_convert_utf8",            &pst::pst_convert_utf8)
        ;
}


