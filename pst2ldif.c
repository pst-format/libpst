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

#define VERSION "0.2"
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
char *folded(char *str);
void  multi(char *fmt, char *str);
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

                if (item->folder && d_ptr->child && strcasecmp(item->file_as, "Deleted Items")) {
                    //if this is a non-empty folder other than deleted items, we want to recurse into it
                    fprintf(stderr, "entering folder %s\n", item->file_as);
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
//                            fprintf(stderr, "\n\n\n");
//                            fprintf(stderr, "access_method %s\n",              item->contact->access_method);
//                            fprintf(stderr, "account_name %s\n",               item->contact->account_name);
//                            fprintf(stderr, "address1 %s\n",                   item->contact->address1);
//                            fprintf(stderr, "address1_desc %s\n",              item->contact->address1_desc);
//                            fprintf(stderr, "address1_transport %s\n",         item->contact->address1_transport);
//                            fprintf(stderr, "address2 %s\n",                   item->contact->address2);
//                            fprintf(stderr, "address2_desc %s\n",              item->contact->address2_desc);
//                            fprintf(stderr, "address2_transport %s\n",         item->contact->address2_transport);
//                            fprintf(stderr, "address3 %s\n",                   item->contact->address3);
//                            fprintf(stderr, "address3_desc %s\n",              item->contact->address3_desc);
//                            fprintf(stderr, "address3_transport %s\n",         item->contact->address3_transport);
//                            fprintf(stderr, "assistant_name %s\n",             item->contact->assistant_name);
//                            fprintf(stderr, "assistant_phone %s\n",            item->contact->assistant_phone);
//                            fprintf(stderr, "billing_information %s\n",        item->contact->billing_information);
//                            fprintf(stderr, "business_address %s\n",           item->contact->business_address);
//                            fprintf(stderr, "business_city %s\n",              item->contact->business_city);
//                            fprintf(stderr, "business_country %s\n",           item->contact->business_country);
//                            fprintf(stderr, "business_fax %s\n",               item->contact->business_fax);
//                            fprintf(stderr, "business_homepage %s\n",          item->contact->business_homepage);
//                            fprintf(stderr, "business_phone %s\n",             item->contact->business_phone);
//                            fprintf(stderr, "business_phone2 %s\n",            item->contact->business_phone2);
//                            fprintf(stderr, "business_po_box %s\n",            item->contact->business_po_box);
//                            fprintf(stderr, "business_postal_code %s\n",       item->contact->business_postal_code);
//                            fprintf(stderr, "business_state %s\n",             item->contact->business_state);
//                            fprintf(stderr, "business_street %s\n",            item->contact->business_street);
//                            fprintf(stderr, "callback_phone %s\n",             item->contact->callback_phone);
//                            fprintf(stderr, "car_phone %s\n",                  item->contact->car_phone);
//                            fprintf(stderr, "company_main_phone %s\n",         item->contact->company_main_phone);
//                            fprintf(stderr, "company_name %s\n",               item->contact->company_name);
//                            fprintf(stderr, "computer_name %s\n",              item->contact->computer_name);
//                            fprintf(stderr, "customer_id %s\n",                item->contact->customer_id);
//                            fprintf(stderr, "def_postal_address %s\n",         item->contact->def_postal_address);
//                            fprintf(stderr, "department %s\n",                 item->contact->department);
//                            fprintf(stderr, "display_name_prefix %s\n",        item->contact->display_name_prefix);
//                            fprintf(stderr, "first_name %s\n",                 item->contact->first_name);
//                            fprintf(stderr, "followup %s\n",                   item->contact->followup);
//                            fprintf(stderr, "free_busy_address %s\n",          item->contact->free_busy_address);
//                            fprintf(stderr, "ftp_site %s\n",                   item->contact->ftp_site);
//                            fprintf(stderr, "fullname %s\n",                   item->contact->fullname);
//                            fprintf(stderr, "gov_id %s\n",                     item->contact->gov_id);
//                            fprintf(stderr, "hobbies %s\n",                    item->contact->hobbies);
//                            fprintf(stderr, "home_address %s\n",               item->contact->home_address);
//                            fprintf(stderr, "home_city %s\n",                  item->contact->home_city);
//                            fprintf(stderr, "home_country %s\n",               item->contact->home_country);
//                            fprintf(stderr, "home_fax %s\n",                   item->contact->home_fax);
//                            fprintf(stderr, "home_phone %s\n",                 item->contact->home_phone);
//                            fprintf(stderr, "home_phone2 %s\n",                item->contact->home_phone2);
//                            fprintf(stderr, "home_po_box %s\n",                item->contact->home_po_box);
//                            fprintf(stderr, "home_postal_code %s\n",           item->contact->home_postal_code);
//                            fprintf(stderr, "home_state %s\n",                 item->contact->home_state);
//                            fprintf(stderr, "home_street %s\n",                item->contact->home_street);
//                            fprintf(stderr, "initials %s\n",                   item->contact->initials);
//                            fprintf(stderr, "isdn_phone %s\n",                 item->contact->isdn_phone);
//                            fprintf(stderr, "job_title %s\n",                  item->contact->job_title);
//                            fprintf(stderr, "keyword %s\n",                    item->contact->keyword);
//                            fprintf(stderr, "language %s\n",                   item->contact->language);
//                            fprintf(stderr, "location %s\n",                   item->contact->location);
//                            fprintf(stderr, "manager_name %s\n",               item->contact->manager_name);
//                            fprintf(stderr, "middle_name %s\n",                item->contact->middle_name);
//                            fprintf(stderr, "mileage %s\n",                    item->contact->mileage);
//                            fprintf(stderr, "mobile_phone %s\n",               item->contact->mobile_phone);
//                            fprintf(stderr, "nickname %s\n",                   item->contact->nickname);
//                            fprintf(stderr, "office_loc %s\n",                 item->contact->office_loc);
//                            fprintf(stderr, "org_id %s\n",                     item->contact->org_id);
//                            fprintf(stderr, "other_address %s\n",              item->contact->other_address);
//                            fprintf(stderr, "other_city %s\n",                 item->contact->other_city);
//                            fprintf(stderr, "other_country %s\n",              item->contact->other_country);
//                            fprintf(stderr, "other_phone %s\n",                item->contact->other_phone);
//                            fprintf(stderr, "other_po_box %s\n",               item->contact->other_po_box);
//                            fprintf(stderr, "other_postal_code %s\n",          item->contact->other_postal_code);
//                            fprintf(stderr, "other_state %s\n",                item->contact->other_state);
//                            fprintf(stderr, "other_street %s\n",               item->contact->other_street);
//                            fprintf(stderr, "pager_phone %s\n",                item->contact->pager_phone);
//                            fprintf(stderr, "personal_homepage %s\n",          item->contact->personal_homepage);
//                            fprintf(stderr, "pref_name %s\n",                  item->contact->pref_name);
//                            fprintf(stderr, "primary_fax %s\n",                item->contact->primary_fax);
//                            fprintf(stderr, "primary_phone %s\n",              item->contact->primary_phone);
//                            fprintf(stderr, "profession %s\n",                 item->contact->profession);
//                            fprintf(stderr, "radio_phone %s\n",                item->contact->radio_phone);
//                            fprintf(stderr, "spouse_name %s\n",                item->contact->spouse_name);
//                            fprintf(stderr, "suffix %s\n",                     item->contact->suffix);
//                            fprintf(stderr, "surname %s\n",                    item->contact->surname);
//                            fprintf(stderr, "telex %s\n",                      item->contact->telex);
//                            fprintf(stderr, "transmittable_display_name %s\n", item->contact->transmittable_display_name);
//                            fprintf(stderr, "ttytdd_phone %s\n",               item->contact->ttytdd_phone);
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
    printf("\t-c class   \t- set the class of the ldap objects\n");
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


char *folded(char *str) {
    if (!str) return "";
    char *ret = rfc2426_escape(str);
    char *n = ret;
    while (n = strchr(n, '\n')) {
        *n = ' ';
    }
    n = ret;
    while (n = strchr(n, ',')) {
        *n = ' ';
    }
    return ret;
}


void multi(char *fmt, char *str) {
    if (!str) return;
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

