/*

Copyright (c) 2004 Carl Byington - 510 Software Group, released under
the GPL version 2 or any later version at your choice available at
http://www.fsf.org/licenses/gpl.txt

Based on readpst.c by David Smith

*/

using namespace std;

// needed for std c++ collections
#include <set>

extern "C" {
    #include "define.h"
    #include "libstrfunc.h"
    #include "libpst.h"
    #include "common.h"
    #include "timeconv.h"
    #include "lzfu.h"
    #include "stdarg.h"
    #include "iconv.h"
}

int32_t     usage();
int32_t     version();
char       *check_filename(char *fname);
char       *dn_escape(const char *str);
void        print_ldif(const char *dn, const char *value);
void        print_ldif_single(const char *dn, const char *value);
void        print_ldif_multi(const char *dn, const char *value);
void        print_ldif_two(const char *dn, const char *value1, const char *value2);
void        build_cn(char *cn, size_t len, int nvalues, char *value, ...);

char *prog_name;
pst_file pstfile;
char *ldap_base  = NULL;    // 'o=some.domain.tld, c=US'
char *ldap_class = NULL;    // 'newPerson'
char *ldap_org   = NULL;    // 'some.domain.tld', computed from ldap_base
iconv_t cd       = 0;       // Character set conversion descriptor


////////////////////////////////////////////////
// define our ordering
struct ltstr {
    bool operator()(const char* s1, const char* s2) const {
        return strcasecmp(s1, s2) < 0;
    }
};
// define our set
typedef set<const char *, ltstr>    string_set;
// make a static set to hold the cn values
static string_set all_strings;


////////////////////////////////////////////////
// helper to free all the strings in a set
//
static void free_strings(string_set &s);
static void free_strings(string_set &s)
{
    for (string_set::iterator i=s.begin(); i!=s.end(); i++) {
        free((void*)*i);
    }
    s.clear();
}


////////////////////////////////////////////////
// helper to register a string in a string set
//
static const char* register_string(string_set &s, const char *name);
static const char* register_string(string_set &s, const char *name) {
    string_set::const_iterator i = s.find(name);
    if (i != s.end()) return *i;
    char *x = strdup(name);
    s.insert(x);
    return x;
}


////////////////////////////////////////////////
// register a global string
//
static const char* register_string(const char *name);
static const char* register_string(const char *name) {
    return register_string(all_strings, name);
}


////////////////////////////////////////////////
// make a unique string
//
static const char* unique_string(const char *name);
static const char* unique_string(const char *name) {
    int  unique = 2;
    string_set::iterator i = all_strings.find(name);
    if (i == all_strings.end()) return register_string(name);
    while (true) {
        char n[strlen(name)+10];
        snprintf(n, sizeof(n), "%s %d", name, unique++);
        string_set::iterator i = all_strings.find(n);
        if (i == all_strings.end()) return register_string(n);
    }
}


static void process(pst_desc_ll *d_ptr);
static void process(pst_desc_ll *d_ptr) {
    pst_item *item = NULL;
    while (d_ptr) {
        if (d_ptr->desc) {
            item = pst_parse_item(&pstfile, d_ptr);
            DEBUG_INFO(("item pointer is %p\n", item));
            if (item) {
                if (item->folder && d_ptr->child && strcasecmp(item->file_as, "Deleted Items")) {
                    //if this is a non-empty folder other than deleted items, we want to recurse into it
                    fprintf(stderr, "entering folder %s\n", item->file_as);
                    process(d_ptr->child);

                } else if (item->contact && (item->type == PST_TYPE_CONTACT)) {
                    // deal with a contact
                    char cn[1000];
                    build_cn(cn, sizeof(cn), 4,
                        item->contact->display_name_prefix,
                        item->contact->first_name,
                        item->contact->surname,
                        item->contact->suffix);
                    if (cn[0] != 0) {
                        // have a valid cn
                        const char *ucn = unique_string(cn);
                        char dn[strlen(ucn) + strlen(ldap_base) + 6];

                        sprintf(dn, "cn=%s, %s", ucn, ldap_base);
                        print_ldif_single("dn", dn);
                        print_ldif_single("cn", ucn);
                        if (item->contact->first_name) {
                            print_ldif_two("givenName",
                                           item->contact->display_name_prefix,
                                           item->contact->first_name);
                        }
                        if (item->contact->surname) {
                            print_ldif_two("sn",
                                           item->contact->surname,
                                           item->contact->suffix);
                        }
                        else if (item->contact->company_name) {
                            print_ldif_single("sn", item->contact->company_name);
                        }
                        else
                            print_ldif_single("sn", ucn); // use cn as sn if we cannot find something better

                        if (item->contact->job_title)
                            print_ldif_single("personalTitle", item->contact->job_title);
                        if (item->contact->company_name)
                            print_ldif_single("company", item->contact->company_name);
                        if (item->contact->address1  && *item->contact->address1)
                            print_ldif_single("mail", item->contact->address1);
                        if (item->contact->address2  && *item->contact->address2)
                            print_ldif_single("mail", item->contact->address2);
                        if (item->contact->address3  && *item->contact->address3)
                            print_ldif_single("mail", item->contact->address3);
                        if (item->contact->address1a && *item->contact->address1a)
                            print_ldif_single("mail", item->contact->address1a);
                        if (item->contact->address2a && *item->contact->address2a)
                            print_ldif_single("mail", item->contact->address2a);
                        if (item->contact->address3a && *item->contact->address3a)
                            print_ldif_single("mail", item->contact->address3a);
                        if (item->contact->business_address) {
                            if (item->contact->business_po_box)
                                print_ldif_single("postalAddress", item->contact->business_po_box);
                            if (item->contact->business_street)
                                print_ldif_multi("postalAddress", item->contact->business_street);
                            if (item->contact->business_city)
                                print_ldif_single("l", item->contact->business_city);
                            if (item->contact->business_state)
                                print_ldif_single("st", item->contact->business_state);
                            if (item->contact->business_postal_code)
                                print_ldif_single("postalCode", item->contact->business_postal_code);
                        }
                        else if (item->contact->home_address) {
                            if (item->contact->home_po_box)
                                print_ldif_single("postalAddress", item->contact->home_po_box);
                            if (item->contact->home_street)
                                print_ldif_multi("postalAddress", item->contact->home_street);
                            if (item->contact->home_city)
                                print_ldif_single("l", item->contact->home_city);
                            if (item->contact->home_state)
                                print_ldif_single("st", item->contact->home_state);
                            if (item->contact->home_postal_code)
                                print_ldif_single("postalCode", item->contact->home_postal_code);
                        }
                        else if (item->contact->other_address) {
                            if (item->contact->other_po_box)
                                print_ldif_single("postalAddress", item->contact->other_po_box);
                            if (item->contact->other_street)
                                print_ldif_multi("postalAddress", item->contact->other_street);
                            if (item->contact->other_city)
                                print_ldif_single("l", item->contact->other_city);
                            if (item->contact->other_state)
                                print_ldif_single("st", item->contact->other_state);
                            if (item->contact->other_postal_code)
                                print_ldif_single("postalCode", item->contact->other_postal_code);
                        }
                        if (item->contact->business_fax)
                            print_ldif_single("facsimileTelephoneNumber", item->contact->business_fax);
                        else if (item->contact->home_fax)
                            print_ldif_single("facsimileTelephoneNumber", item->contact->home_fax);

                        if (item->contact->business_phone)
                            print_ldif_single("telephoneNumber", item->contact->business_phone);
                        if (item->contact->home_phone)
                            print_ldif_single("homePhone", item->contact->home_phone);

                        if (item->contact->car_phone)
                            print_ldif_single("mobile", item->contact->car_phone);
                        else if (item->contact->mobile_phone)
                            print_ldif_single("mobile", item->contact->mobile_phone);
                        else if (item->contact->other_phone)
                            print_ldif_single("mobile", item->contact->other_phone);


                        if (item->comment)
                            print_ldif_single("description", item->comment);

                        print_ldif("objectClass", ldap_class);
                        putchar('\n');
                    }
                }
                else {
                    DEBUG_INFO(("item is not a contact\n"));
                }
            }
            pst_freeItem(item);
        }
        d_ptr = d_ptr->next;
    }
}


void print_ldif(const char *dn, const char *value)
{
    printf("%s: %s\n", dn, value);
}


// Prints a Distinguished Name together with its value.
// If the value isn't a "SAFE STRING" (as defined in RFC2849),
// then it is output as a BASE-64 encoded value
void print_ldif_single(const char *dn, const char *value)
{
    size_t len;
    bool is_safe_string = true;
    bool needs_code_conversion = false;
    bool space_flag = false;

    // Strip leading spaces
    while (*value == ' ') value++;
    len = strlen(value) + 1;
    char buffer[len];
    char *p = buffer;
    // See if "value" is a "SAFE STRING"

    // First check characters that are safe but not safe as initial characters
    if (*value == ':' || *value == '<')
        is_safe_string = false;
    for (;;) {
        char ch = *value++;

        if (ch == 0 || ch == '\n')
            break;
        else if (ch == '\r')
            continue;
        else if (ch == ' ') {
            space_flag = true;
            continue;
        }
        else {
            if ((ch & 0x80) == 0x80) {
                needs_code_conversion = true;
                is_safe_string = false;
            }
            if (space_flag) {
                *p++ = ' ';
                space_flag = false;
            }
            *p++ = ch;
        }
    }
    *p = 0;
    if (is_safe_string) {
        printf("%s: %s\n", dn, buffer);
        return;
    }

    if (needs_code_conversion && cd != 0) {
        size_t inlen = p - buffer;
        size_t utf8_len = 2 * inlen + 1;
        char utf8_buffer[utf8_len];
        char *utf8_p = utf8_buffer;

        iconv(cd, NULL, NULL, NULL, NULL);
        p = buffer;
        int ret = iconv(cd, &p, &inlen, &utf8_p, &utf8_len);

        if (ret >= 0) {
            *utf8_p = 0;
            p = base64_encode(utf8_buffer, utf8_p - utf8_buffer);
        }
        else
            p = base64_encode(buffer, strlen(buffer));
    }
    else
        p = base64_encode(buffer, strlen(buffer));
    printf("%s:: %s\n", dn, p);
    free(p);
}


void print_ldif_multi(const char *dn, const char *value)
{
    const char *n;
    while ((n = strchr(value, '\n'))) {
        print_ldif_single(dn, value);
        value = n + 1;
    }
    print_ldif_single(dn, value);
}


void print_ldif_two(const char *dn, const char *value1, const char *value2)
{
    size_t len1, len2;
    if (value1 && *value1)
        len1 = strlen(value1);
    else {
        print_ldif_single(dn, value2);
        return;
    }

    if (value2 && *value2)
        len2 = strlen(value2);
    else {
        print_ldif_single(dn, value1);
        return;
    }

    char value[len1 + len2 + 2];
    memcpy(value, value1, len1);
    value[len1] = ' ';
    memcpy(value + len1 + 1, value2, len2 + 1);
    print_ldif_single(dn, value);
}


void build_cn(char *cn, size_t len, int nvalues, char *value, ...)
{
    bool space_flag = false;
    int i = 0;
    va_list ap;

    va_start(ap, value);

    while (!value) {
       nvalues--;
       if (nvalues == 0) {
           va_end(ap);
           return;
       }
       value = va_arg(ap, char *);
    }
    for (;;) {
        char ch = *value++;

        if (ch == 0 || ch == '\n') {
            do {
                value = NULL;
                nvalues--;
                if (nvalues == 0) break;
                value = va_arg(ap, char *);
            } while (!value);
            if (!value) break;
            space_flag = true;
        }
        else if (ch == '\r')
            continue;
        else if (ch == ' ') {
            space_flag = true;
            continue;
        }
        else {
            if (space_flag) {
                if (i > 0) {
                    if (i < (len - 2)) cn[i++] = ' ';
                    else               break;
                }
                space_flag = false;
            }
            if (i < (len - 1)) cn[i++] = ch;
            else               break;
        }
    }
    cn[i] = 0;
    va_end(ap);
}


int main(int argc, char** argv) {
    pst_desc_ll *d_ptr;
    char *fname = NULL;
    char *temp = NULL;        //temporary char pointer
    int c;
    char *d_log = NULL;
    prog_name = argv[0];
    pst_item *item = NULL;

    while ((c = getopt(argc, argv, "b:c:C:d:Vh"))!= -1) {
        switch (c) {
        case 'b':
            ldap_base = optarg;
            temp = strchr(ldap_base, ',');
            if (temp) {
                *temp = '\0';
                ldap_org = strdup(ldap_base+2); // assume first 2 chars are o=
                *temp = ',';
            }
            break;
        case 'c':
            ldap_class = optarg;
            break;
        case 'C':
            cd = iconv_open("UTF-8", optarg);
            if (cd == (iconv_t)(-1)) {
                fprintf(stderr, "I don't know character set \"%s\"!\n\n", optarg);
                fprintf(stderr, "Type: \"iconv --list\" to get list of known character sets\n");
                return 1;
            }
            break;
        case 'd':
            d_log = optarg;
            break;
        case 'h':
            usage();
            exit(0);
            break;
        case 'V':
            version();
            exit(0);
            break;
        default:
            usage();
            exit(1);
            break;
        }
    }

    if ((argc > optind) && (ldap_base) && (ldap_class) && (ldap_org)) {
        fname = argv[optind];
    } else {
        usage();
        exit(2);
    }

    #ifdef DEBUG_ALL
        // force a log file
        if (!d_log) d_log = "pst2ldif.log";
    #endif
    DEBUG_INIT(d_log);
    DEBUG_REGISTER_CLOSE();
    DEBUG_ENT("main");
    RET_DERROR(pst_open(&pstfile, fname), 1, ("Error opening File\n"));
    RET_DERROR(pst_load_index(&pstfile), 2, ("Index Error\n"));

    pst_load_extended_attributes(&pstfile);

    d_ptr = pstfile.d_head; // first record is main record
    item  = (pst_item*)pst_parse_item(&pstfile, d_ptr);
    if (!item || !item->message_store) {
        DEBUG_RET();
        DIE(("main: Could not get root record\n"));
    }

    d_ptr = pst_getTopOfFolders(&pstfile, item);
    if (!d_ptr) {
        DEBUG_RET();
        DIE(("Top of folders record not found. Cannot continue\n"));
    }

    pst_freeItem(item);

    // write the ldap header
    printf("dn: %s\n", ldap_base);
    printf("o: %s\n", ldap_org);
    printf("objectClass: organization\n\n");
    printf("dn: cn=root, %s\n", ldap_base);
    printf("cn: root\n");
    printf("objectClass: %s\n\n", ldap_class);

    process(d_ptr->child);  // do the children of TOPF
    pst_close(&pstfile);
    DEBUG_RET();
    free_strings(all_strings);
    return 0;
}


int usage() {
    version();
    printf("Usage: %s [OPTIONS] {PST FILENAME}\n", prog_name);
    printf("OPTIONS:\n");
    printf("\t-h\t- Help. This screen\n");
    printf("\t-V\t- Version. Display program version\n");
    printf("\t-b ldapbase\t- set the LDAP base value\n");
    printf("\t-c class   \t- set the class of the LDAP objects\n");
    printf("\t-C charset \t- assumed character set of non-ASCII characters\n");
    return 0;
}


int version() {
    printf("pst2ldif v%s\n", VERSION);
#if BYTE_ORDER == BIG_ENDIAN
    printf("Big Endian implementation being used.\n");
#elif BYTE_ORDER == LITTLE_ENDIAN
    printf("Little Endian implementation being used.\n");
#else
#  error "Byte order not supported by this library"
#endif
#ifdef __GNUC__
    printf("GCC %d.%d : %s %s\n", __GNUC__, __GNUC_MINOR__, __DATE__, __TIME__);
#endif
    return 0;
}


char *check_filename(char *fname) {
    char *t = fname;
    if (t == NULL) {
        return fname;
    }
    while ((t = strpbrk(t, "/\\:"))) {
        // while there are characters in the second string that we don't want
        *t = '_'; //replace them with an underscore
    }
    return fname;
}

#if 0
// This function escapes Distinguished Names (as per RFC4514)
char *dn_escape(const char *str) {
    static char* buf = NULL;
    const char *a;
    char *ret, *b;
    if (str == NULL)
        ret = NULL;
    else {
        // Calculate maximum space needed (if every character must be escaped)
        int x = 2 * strlen(str) + 1;  // don't forget room for the NUL
        buf = (char*) realloc(buf, x);
        a = str;
        b = buf;

        // remove leading spaces (RFC says escape them)
        while (*a == ' ')
            a++;

        // escape initial '#'
        if (*a == '#')
            *b++ = '\\';

        while (*a != '\0') {
            switch(*a) {
                case '\\':
                case '"' :
                case '+' :
                case ';' :
                case '<' :
                case '>' :
                    *(b++)='\\';
                    *b=*a;
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
    return ret;
}
#endif
