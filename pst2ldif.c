/*

Copyright (c) 2004 Carl Byington - 510 Software Group, released under
the GPL version 2 or any later version at your choice available at
http://www.fsf.org/licenses/gpl.txt

Based on readpst.c by David Smith

*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

#ifndef _WIN32
# include <unistd.h>
# include <sys/stat.h> //mkdir

// for reading of directory and clearing in function mk_separate_dir
# include <sys/types.h>
# include <dirent.h>
#else
# include <direct.h>
# define chdir _chdir
# define int32_t __int32
#endif

#ifndef __GNUC__
# include "XGetopt.h"
#endif

#include "libstrfunc.h" // for base64_encoding

#include "define.h"
#include "libpst.h"
#include "common.h"
#include "timeconv.h"
#include "lzfu.h"

#define OUTPUT_TEMPLATE "%s"

#define VERSION "0.1"
// max size of the c_time char*. It will store the date of the email
#define C_TIME_SIZE 500
#define PERM_DIRS 0777

// macro used for creating directories
#ifndef WIN32
#define D_MKDIR(x) mkdir(x, PERM_DIRS)
#else
#define D_MKDIR(x) mkdir(x)
#endif

int32_t   usage();
int32_t   version();
char *my_stristr(char *haystack, char *needle);
char *check_filename(char *fname);
char *single(char *str);
char *multi(char *fmt, char *str);
char *rfc2426_escape(char *str);
int32_t chr_count(char *str, char x);

char *prog_name;
pst_file pstfile;
char *ldap_base  = NULL;  // 'o=some.domain.tld, c=US'
char *ldap_class = NULL;  // 'newPerson'
char *ldap_org   = NULL;  // 'o=some.domain.tld', computed from ldap_base


char *trim(char *name);
char *trim(char *name) {
    char *p;
    while (*name == ' ') name++;
    p = name + strlen(name) - 1;
    while ((p >= name) && (*p == ' ')) *p-- = '\0';
    return name;
}


void process(pst_desc_ll *d_ptr);
void process(pst_desc_ll *d_ptr) {
    pst_item *item = NULL;
    while (d_ptr) {
        if (d_ptr->desc) {
            item = _pst_parse_item(&pstfile, d_ptr);
            if (item) {
                if (item->message_store) {
                    // there should only be one message_store, and we have already done it
                    DIE(("main: A second message_store has been found. Sorry, this must be an error.\n"));
                }

                if (item->folder) { //if this is a folder, we want to recurse into it
                    process(d_ptr->child);
                } else if (item->contact) {
                    // deal with a contact
                    if (item->type != PST_TYPE_CONTACT) {
                        DIE(("type should be contact\n"));
                    }
                    else if (item->contact == NULL) { // this is an incorrect situation. Inform user
                        DIE(("null item contact\n"));
                    } else {
                        char cn[1000];
                        snprintf(cn, sizeof(cn), "%s %s %s %s",
                            single(item->contact->display_name_prefix),
                            single(item->contact->first_name),
                            single(item->contact->surname),
                            single(item->contact->suffix));
                        if (strcmp(cn, "   ")) {
                            // have a valid cn
                            printf("dn: cn=%s, %s\n", trim(cn), ldap_base);
                            printf("cn: %s\n", trim(cn));
                            if (item->contact->first_name) {
                                snprintf(cn, sizeof(cn), "%s %s",
                                    single(item->contact->display_name_prefix),
                                    single(item->contact->first_name));
                                printf("givenName: %s\n", trim(cn));
                            }
                            if (item->contact->surname) {
                                snprintf(cn, sizeof(cn), "%s %s",
                                    single(item->contact->surname),
                                    single(item->contact->suffix));
                                printf("sn: %s\n", trim(cn));
                            }
                            if (item->contact->job_title)
                                printf("personalTitle: %s\n", single(item->contact->job_title));
                            if (item->contact->company_name)
                                printf("company: %s\n", single(item->contact->company_name));
                            if (item->contact->address1)
                                printf("mail: %s\n", single(item->contact->address1));
                            if (item->contact->address2)
                                printf("mail: %s\n", single(item->contact->address2));
                            if (item->contact->address3)
                                printf("mail: %s\n", single(item->contact->address3));
                            if (item->contact->business_address) {
                                if (item->contact->business_po_box)
                                    printf("postalAddress: %s\n", single(item->contact->business_po_box));
                                if (item->contact->business_street)
                                    multi("postalAddress: %s\n", item->contact->business_street);
                                if (item->contact->business_city)
                                    printf("l: %s\n", single(item->contact->business_city));
                                if (item->contact->business_state)
                                    printf("st: %s\n", single(item->contact->business_state));
                                if (item->contact->business_postal_code)
                                    printf("postalCode: %s\n", single(item->contact->business_postal_code));
                            }
                            else if (item->contact->home_address) {
                                if (item->contact->home_po_box)
                                    printf("postalAddress: %s\n", single(item->contact->home_po_box));
                                if (item->contact->home_street)
                                    multi("postalAddress: %s\n", item->contact->home_street);
                                if (item->contact->home_city)
                                    printf("l: %s\n", single(item->contact->home_city));
                                if (item->contact->home_state)
                                    printf("st: %s\n", single(item->contact->home_state));
                                if (item->contact->home_postal_code)
                                    printf("postalCode: %s\n", single(item->contact->home_postal_code));
                            }
                            else if (item->contact->other_address) {
                                if (item->contact->other_po_box)
                                    printf("postalAddress: %s\n", single(item->contact->other_po_box));
                                if (item->contact->other_street)
                                    multi("postalAddress: %s\n", item->contact->other_street);
                                if (item->contact->other_city)
                                    printf("l: %s\n", single(item->contact->other_city));
                                if (item->contact->other_state)
                                    printf("st: %s\n", single(item->contact->other_state));
                                if (item->contact->other_postal_code)
                                    printf("postalCode: %s\n", single(item->contact->other_postal_code));
                            }
                            if (item->contact->business_fax)
                                printf("facsimileTelephoneNumber: %s\n", single(item->contact->business_fax));
                            else if (item->contact->home_fax)
                                printf("facsimileTelephoneNumber: %s\n", single(item->contact->home_fax));

                            if (item->contact->business_phone)
                                printf("telephoneNumber: %s\n", single(item->contact->business_phone));
                            if (item->contact->home_phone)
                                printf("homePhone: %s\n", single(item->contact->home_phone));

                            if (item->contact->car_phone)
                                printf("mobile: %s\n", single(item->contact->car_phone));
                            else if (item->contact->mobile_phone)
                                printf("mobile: %s\n", single(item->contact->mobile_phone));
                            else if (item->contact->other_phone)
                                printf("mobile: %s\n", single(item->contact->other_phone));


                            if (item->comment)
                                printf("description: %s\n", single(item->comment));

                            printf("objectClass: %s\n\n", ldap_class);
                        }
                    }
                }
            }
            _pst_freeItem(item);
        }
        d_ptr = d_ptr->next;
    }
}


int main(int argc, char** argv) {
    pst_desc_ll *d_ptr;
    char *fname = NULL;
    char *temp = NULL;        //temporary char pointer
    char c;
    prog_name = argv[0];
    pst_item *item = NULL;

    while ((c = getopt(argc, argv, "b:c:Vh"))!= -1) {
        switch (c) {
        case 'b':
            ldap_base = optarg;
            temp = strchr(ldap_base, ',');
            if (temp) {
                *temp = '\0';
                ldap_org = strdup(ldap_base);
                *temp = ',';
            }
            break;
        case 'c':
            ldap_class = optarg;
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

    DEBUG_INIT("pst2ldif.log");
    DEBUG_REGISTER_CLOSE();
    DEBUG_ENT("main");
    RET_DERROR(pst_open(&pstfile, fname, "r"), 1, ("Error opening File\n"));
    RET_DERROR(pst_load_index(&pstfile), 2, ("Index Error\n"));

    pst_load_extended_attributes(&pstfile);

    d_ptr = pstfile.d_head; // first record is main record
    item  = _pst_parse_item(&pstfile, d_ptr);
    if (!item || !item->message_store) {
        DIE(("main: Could not get root record\n"));
    }

    d_ptr = pst_getTopOfFolders(&pstfile, item);
    if (!d_ptr) {
        DIE(("Top of folders record not found. Cannot continue\n"));
    }

    _pst_freeItem(item);

    // write the ldap header
    printf("dn: %s\n", ldap_base);
    printf("o: %s\n", ldap_org);
    printf("objectClass: organization\n\n");
    printf("dn: cn=root, %s\n", ldap_base);
    printf("cn: root\n");
    printf("objectClass: %s\n\n", ldap_class);

    process(d_ptr->child);  // do the children of TOPF
    pst_close(&pstfile);
    return 0;
}


int usage() {
    version();
    printf("Usage: %s [OPTIONS] {PST FILENAME}\n", prog_name);
    printf("OPTIONS:\n");
    printf("\t-h\t- Help. This screen\n");
    printf("\t-V\t- Version. Display program version\n");
    printf("\t-b ldapbase\t- set the ldap base value\n");
    printf("\t-b ldapbase\t- set the ldap base value\n");
    return 0;
}


int version() {
    printf("pst2ldif v%s using LibPST v%s\n", VERSION, PST_VERSION);
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


// my_stristr varies from strstr in that its searches are case-insensitive
char * my_stristr(char *haystack, char *needle) {
    char *x=haystack, *y=needle, *z = NULL;
    if (haystack == NULL || needle == NULL)
        return NULL;
    while (*y != '\0' && *x != '\0') {
        if (tolower(*y) == tolower(*x)) {
            // move y on one
            y++;
            if (z == NULL) {
        z = x; // store first position in haystack where a match is made
            }
        } else {
            y = needle; // reset y to the beginning of the needle
            z = NULL; // reset the haystack storage point
        }
        x++; // advance the search in the haystack
    }
    return z;
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


char *single(char *str) {
    if (!str) return "";
    char *ret = rfc2426_escape(str);
    char *n = strchr(ret, '\n');
    if (n) *n = '\0';
    return ret;
}


char *multi(char *fmt, char *str) {
    if (!str) return "";
    char *ret = rfc2426_escape(str);
    char *n = ret;
    while (n = strchr(ret, '\n')) {
        *n = '\0';
        printf(fmt, ret);
        ret = n+1;
    }
    if (*ret) printf(fmt, ret);
}


char *rfc2426_escape(char *str) {
    static char* buf = NULL;
    char *ret, *a, *b;
    int x = 0, y, z;
    if (str == NULL)
        ret = str;
    else {

        // calculate space required to escape all the following characters
        x = strlen(str) +(y=(chr_count(str, ',')*2) + (chr_count(str, '\\')*2) + (chr_count(str, ';')*2) + (chr_count(str, '\n')*2));
        z = chr_count(str, '\r');
        if (y == 0 && z == 0)
            // there isn't any extra space required
            ret = str;
        else {
            buf = (char*) realloc(buf, x+1);
            a = str;
            b = buf;
            while (*a != '\0') {
                switch(*a) {
                 // case ',' :
                    case '\\':
                    case ';' :
                 // case '\n':
                        *(b++)='\\';
                        *b=*a;
                    break;
                    case '\r':
                        break;
                    default:
                        *b=*a;
                }
                b++;
                a++;
            }
            *b = '\0';
            ret = buf;
        }
    }
    return ret;
}


int chr_count(char *str, char x) {
    int r = 0;
    while (*str != '\0') {
        if (*str == x)
            r++;
        str++;
    }
    return r;
}

