/***
 * readpst.c
 * Part of the LibPST project
 * Written by David Smith
 *            dave.s@earthcorp.com
 */

#include "define.h"
#include "lzfu.h"

#define OUTPUT_TEMPLATE "%s"
#define OUTPUT_KMAIL_DIR_TEMPLATE ".%s.directory"
#define KMAIL_INDEX ".%s.index"
#define SEP_MAIL_FILE_TEMPLATE "%i" /* "%09i" */

// max size of the c_time char*. It will store the date of the email
#define C_TIME_SIZE 500

struct file_ll {
    char *name;
    char *dname;
    FILE * output;
    int32_t stored_count;
    int32_t email_count;
    int32_t skip_count;
    int32_t type;
};

void      process(pst_item *outeritem, pst_desc_ll *d_ptr);
void      write_email_body(FILE *f, char *body);
void      removeCR(char *c);
void      usage();
void      version();
char*     mk_kmail_dir(char*);
int       close_kmail_dir();
char*     mk_recurse_dir(char*);
int       close_recurse_dir();
char*     mk_separate_dir(char *dir);
int       close_separate_dir();
int       mk_separate_file(struct file_ll *f);
char*     my_stristr(char *haystack, char *needle);
void      check_filename(char *fname);
void      write_separate_attachment(char f_name[], pst_item_attach* attach, int attach_num, pst_file* pst);
void      write_embedded_message(FILE* f_output, pst_item_attach* attach, char *boundary, pst_file* pf, char** extra_mime_headers);
void      write_inline_attachment(FILE* f_output, pst_item_attach* attach, char *boundary, pst_file* pst);
void      header_has_field(char *header, char *field, int *flag);
void      header_get_subfield(char *field, const char *subfield, char *body_subfield, size_t size_subfield);
char*     header_get_field(char *header, char *field);
char*     header_end_field(char *field);
void      header_strip_field(char *header, char *field);
int       test_base64(char *body);
void      find_html_charset(char *html, char *charset, size_t charsetlen);
void      find_rfc822_headers(char** extra_mime_headers);
void      write_body_part(FILE* f_output, char *body, char *mime, char *charset, char *boundary);
void      write_normal_email(FILE* f_output, char f_name[], pst_item* item, int mode, int mode_MH, pst_file* pst, int save_rtf, char** extra_mime_headers);
void      write_vcard(FILE* f_output, pst_item_contact* contact, char comment[]);
void      write_appointment(FILE* f_output, pst_item_appointment* appointment,
                            pst_item_email* email, FILETIME* create_date, FILETIME* modify_date);
void      create_enter_dir(struct file_ll* f, pst_item *item);
void      close_enter_dir(struct file_ll *f);

const char*  prog_name;
char*  output_dir = ".";
char*  kmail_chdir = NULL;

// Normal mode just creates mbox format files in the current directory. Each file is named
// the same as the folder's name that it represents
#define MODE_NORMAL 0

// KMail mode creates a directory structure suitable for being used directly
// by the KMail application
#define MODE_KMAIL 1

// recurse mode creates a directory structure like the PST file. Each directory
// contains only one file which stores the emails in mbox format.
#define MODE_RECURSE 2

// separate mode creates the same directory structure as recurse. The emails are stored in
// separate files, numbering from 1 upward. Attachments belonging to the emails are
// saved as email_no-filename (e.g. 1-samplefile.doc or 000001-Attachment2.zip)
#define MODE_SEPARATE 3

// Decrypt the whole file (even the parts that aren't encrypted) and ralph it to stdout
#define MODE_DECSPEW 4


// Output Normal just prints the standard information about what is going on
#define OUTPUT_NORMAL 0

// Output Quiet is provided so that only errors are printed
#define OUTPUT_QUIET 1

// default mime-type for attachments that have a null mime-type
#define MIME_TYPE_DEFAULT "application/octet-stream"
#define RFC822            "message/rfc822"

// output mode for contacts
#define CMODE_VCARD 0
#define CMODE_LIST  1

// output mode for deleted items
#define DMODE_EXCLUDE 0
#define DMODE_INCLUDE 1

// output settings for RTF bodies
// filename for the attachment
#define RTF_ATTACH_NAME "rtf-body.rtf"
// mime type for the attachment
#define RTF_ATTACH_TYPE "application/rtf"

// global settings
int mode = MODE_NORMAL;
int mode_MH = 0;
int output_mode = OUTPUT_NORMAL;
int contact_mode = CMODE_VCARD;
int deleted_mode = DMODE_EXCLUDE;
int overwrite = 0;
int save_rtf_body = 1;
pst_file pstfile;
regex_t  meta_charset_pattern;


void process(pst_item *outeritem, pst_desc_ll *d_ptr)
{
    struct file_ll ff;
    pst_item *item = NULL;

    DEBUG_ENT("process");
    memset(&ff, 0, sizeof(ff));
    create_enter_dir(&ff, outeritem);

    while (d_ptr) {
        DEBUG_MAIN(("main: New item record\n"));
        if (!d_ptr->desc) {
            DEBUG_WARN(("main: ERROR ?? item's desc record is NULL\n"));
            ff.skip_count++;
        }
        else {
            DEBUG_MAIN(("main: Desc Email ID %#x [d_ptr->id = %#x]\n", d_ptr->desc->id, d_ptr->id));

            item = pst_parse_item(&pstfile, d_ptr);
            DEBUG_MAIN(("main: About to process item\n"));
            if (item && item->email && item->email->subject && item->email->subject->subj) {
                DEBUG_EMAIL(("item->email->subject = %p\n", item->email->subject));
                DEBUG_EMAIL(("item->email->subject->subj = %p\n", item->email->subject->subj));
            }
            if (item) {
                if (item->folder && d_ptr->child && (deleted_mode == DMODE_INCLUDE || strcasecmp(item->file_as, "Deleted Items"))) {
                    //if this is a non-empty folder other than deleted items, we want to recurse into it
                    if (output_mode != OUTPUT_QUIET) printf("Processing Folder \"%s\"\n", item->file_as);
                    process(item, d_ptr->child);

                } else if (item->contact && (item->type == PST_TYPE_CONTACT)) {
                    // deal with a contact
                    // write them to the file, one per line in this format
                    // Desc Name <email@address>\n
                    if (mode == MODE_SEPARATE) mk_separate_file(&ff);
                    ff.email_count++;
                    DEBUG_MAIN(("main: Processing Contact\n"));
                    if (ff.type != PST_TYPE_CONTACT) {
                        DEBUG_MAIN(("main: I have a contact, but the folder isn't a contacts folder. Processing anyway\n"));
                    }
                    if (contact_mode == CMODE_VCARD)
                        write_vcard(ff.output, item->contact, item->comment);
                    else
                        fprintf(ff.output, "%s <%s>\n", item->contact->fullname, item->contact->address1);

                } else if (item->email && (item->type == PST_TYPE_NOTE || item->type == PST_TYPE_REPORT || item->type == PST_TYPE_OTHER)) {
                    char *extra_mime_headers = NULL;
                    if (mode == MODE_SEPARATE) mk_separate_file(&ff);
                    ff.email_count++;
                    DEBUG_MAIN(("main: Processing Email\n"));
                    if ((ff.type != PST_TYPE_NOTE) && (ff.type != PST_TYPE_REPORT) && (ff.type != PST_TYPE_OTHER)) {
                        DEBUG_MAIN(("main: I have an email, but the folder isn't an email folder. Processing anyway\n"));
                    }
                    write_normal_email(ff.output, ff.name, item, mode, mode_MH, &pstfile, save_rtf_body, &extra_mime_headers);

                } else if (item->journal && (item->type == PST_TYPE_JOURNAL)) {
                    // deal with journal items
                    if (mode == MODE_SEPARATE) mk_separate_file(&ff);
                    ff.email_count++;
                    DEBUG_MAIN(("main: Processing Journal Entry\n"));
                    if (ff.type != PST_TYPE_JOURNAL) {
                        DEBUG_MAIN(("main: I have a journal entry, but the folder isn't a journal folder. Processing anyway\n"));
                    }
                    fprintf(ff.output, "BEGIN:VJOURNAL\n");
                    if (item->email && item->email->subject && item->email->subject->subj)
                        fprintf(ff.output, "SUMMARY:%s\n", pst_rfc2426_escape(item->email->subject->subj));
                    if (item->email && item->email->body)
                        fprintf(ff.output, "DESCRIPTION:%s\n", pst_rfc2426_escape(item->email->body));
                    if (item->journal->start)
                        fprintf(ff.output, "DTSTART;VALUE=DATE-TIME:%s\n", pst_rfc2445_datetime_format(item->journal->start));
                    fprintf(ff.output, "END:VJOURNAL\n\n");

                } else if (item->appointment && (item->type == PST_TYPE_APPOINTMENT)) {
                    // deal with Calendar appointments
                    if (mode == MODE_SEPARATE) mk_separate_file(&ff);
                    ff.email_count++;
                    DEBUG_MAIN(("main: Processing Appointment Entry\n"));
                    if (ff.type != PST_TYPE_APPOINTMENT) {
                        DEBUG_MAIN(("main: I have an appointment, but folder isn't specified as an appointment type. Processing...\n"));
                    }
                    write_appointment(ff.output, item->appointment, item->email, item->create_date, item->modify_date);

                } else if (item->message_store) {
                    // there should only be one message_store, and we have already done it
                    DEBUG_MAIN(("item with message store content, type %i %s folder type %i, skipping it\n", item->type, item->ascii_type, ff.type));

                } else {
                    // these all seem to be things that MS agrees are not included in the item count
                    //ff.skip_count++;
                    DEBUG_MAIN(("main: Unknown item type %i (%s) name (%s)\n",
                                item->type, item->ascii_type, item->file_as));
                }
                pst_freeItem(item);
            } else {
                ff.skip_count++;
                DEBUG_MAIN(("main: A NULL item was seen\n"));
            }
            d_ptr = d_ptr->next;
        }
    }
    close_enter_dir(&ff);
    DEBUG_RET();
}



int main(int argc, char* const* argv) {
    pst_item *item = NULL;
    pst_desc_ll *d_ptr;
    char * fname = NULL;
    char *d_log  = NULL;
    int c,x;
    char *temp = NULL;               //temporary char pointer
    prog_name = argv[0];

    time_t now = time(NULL);
    srand((unsigned)now);

    if (regcomp(&meta_charset_pattern, "<meta[^>]*content=\"[^>]*charset=([^>\";]*)[\";]", REG_ICASE | REG_EXTENDED)) {
        printf("cannot compile regex pattern to find content charset in html bodies\n");
        exit(3);
    }

    // command-line option handling
    while ((c = getopt(argc, argv, "bCc:Dd:hko:qrSMVw"))!= -1) {
        switch (c) {
        case 'b':
            save_rtf_body = 0;
            break;
        case 'C':
            mode = MODE_DECSPEW;
            break;
        case 'c':
            if (optarg && optarg[0]=='v')
                contact_mode=CMODE_VCARD;
            else if (optarg && optarg[0]=='l')
                contact_mode=CMODE_LIST;
            else {
                usage();
                exit(0);
            }
            break;
        case 'D':
            deleted_mode = DMODE_INCLUDE;
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
        case 'k':
            mode = MODE_KMAIL;
            break;
        case 'M':
            mode = MODE_SEPARATE;
            mode_MH = 1;
            break;
        case 'o':
            output_dir = optarg;
            break;
        case 'q':
            output_mode = OUTPUT_QUIET;
            break;
        case 'r':
            mode = MODE_RECURSE;
            break;
        case 'S':
            mode = MODE_SEPARATE;
            break;
        case 'w':
            overwrite = 1;
            break;
        default:
            usage();
            exit(1);
            break;
        }
    }

    if (argc > optind) {
        fname = argv[optind];
    } else {
        usage();
        exit(2);
    }

    #ifdef DEBUG_ALL
        // force a log file
        if (!d_log) d_log = "readpst.log";
    #endif // defined DEBUG_ALL
    DEBUG_INIT(d_log);
    DEBUG_REGISTER_CLOSE();
    DEBUG_ENT("main");

    if (mode == MODE_DECSPEW) {
        FILE  *fp;
        char   buf[1024];
        size_t l = 0;
        if (NULL == (fp = fopen(fname, "rb"))) {
            fprintf(stderr, "Couldn't open file %s\n", fname );
            DEBUG_RET();
            return 1;
        }

        while (0 != (l = fread(buf, 1, 1024, fp))) {
            if (0 != pst_decrypt(0, buf, l, PST_COMP_ENCRYPT))
                fprintf(stderr, "pst_decrypt() failed (I'll try to continue)\n");

            if (l != pst_fwrite(buf, 1, l, stdout)) {
                fprintf(stderr, "Couldn't output to stdout?\n");
                DEBUG_RET();
                return 1;
            }
        }
        DEBUG_RET();
        return 0;
    }

    if (output_mode != OUTPUT_QUIET) printf("Opening PST file and indexes...\n");

    RET_DERROR(pst_open(&pstfile, fname), 1, ("Error opening File\n"));
    RET_DERROR(pst_load_index(&pstfile), 2, ("Index Error\n"));

    pst_load_extended_attributes(&pstfile);

    if (chdir(output_dir)) {
        x = errno;
        pst_close(&pstfile);
        DEBUG_RET();
        DIE(("main: Cannot change to output dir %s: %s\n", output_dir, strerror(x)));
    }

    if (output_mode != OUTPUT_QUIET) printf("About to start processing first record...\n");

    d_ptr = pstfile.d_head; // first record is main record
    item  = pst_parse_item(&pstfile, d_ptr);
    if (!item || !item->message_store) {
        DEBUG_RET();
        DIE(("main: Could not get root record\n"));
    }

    // default the file_as to the same as the main filename if it doesn't exist
    if (!item->file_as) {
        if (!(temp = strrchr(fname, '/')))
            if (!(temp = strrchr(fname, '\\')))
                temp = fname;
            else
                temp++; // get past the "\\"
        else
            temp++; // get past the "/"
        item->file_as = (char*)xmalloc(strlen(temp)+1);
        strcpy(item->file_as, temp);
        DEBUG_MAIN(("file_as was blank, so am using %s\n", item->file_as));
    }
    DEBUG_MAIN(("main: Root Folder Name: %s\n", item->file_as));

    d_ptr = pst_getTopOfFolders(&pstfile, item);
    if (!d_ptr) {
        DEBUG_RET();
        DIE(("Top of folders record not found. Cannot continue\n"));
    }

    process(item, d_ptr->child);    // do the children of TOPF
    pst_freeItem(item);
    pst_close(&pstfile);
    DEBUG_RET();
    regfree(&meta_charset_pattern);
    return 0;
}


void write_email_body(FILE *f, char *body) {
    char *n = body;
    //  DEBUG_MAIN(("write_email_body(): \"%s\"\n", body));
    DEBUG_ENT("write_email_body");
    while (n) {
        if (strncmp(body, "From ", 5) == 0)
            fprintf(f, ">");
        if ((n = strchr(body, '\n'))) {
            n++;
            pst_fwrite(body, n-body, 1, f); //write just a line
            body = n;
        }
    }
    pst_fwrite(body, strlen(body), 1, f);
    DEBUG_RET();
}


void removeCR (char *c) {
    // converts \r\n to \n
    char *a, *b;
    DEBUG_ENT("removeCR");
    a = b = c;
    while (*a != '\0') {
        *b = *a;
        if (*a != '\r') b++;
        a++;
    }
    *b = '\0';
    DEBUG_RET();
}


void usage() {
    DEBUG_ENT("usage");
    version();
    printf("Usage: %s [OPTIONS] {PST FILENAME}\n", prog_name);
    printf("OPTIONS:\n");
    printf("\t-V\t- Version. Display program version\n");
    printf("\t-C\t- Decrypt (compressible encryption) the entire file and output on stdout (not typically useful)\n");
    printf("\t-D\t- Include deleted items in output\n");
    printf("\t-M\t- MH. Write emails in the MH format\n");
    printf("\t-S\t- Separate. Write emails in the separate format\n");
    printf("\t-b\t- Don't save RTF-Body attachments\n");
    printf("\t-c[v|l]\t- Set the Contact output mode. -cv = VCard, -cl = EMail list\n");
    printf("\t-d <filename> \t- Debug to file. This is a binary log. Use readpstlog to print it\n");
    printf("\t-h\t- Help. This screen\n");
    printf("\t-k\t- KMail. Output in kmail format\n");
    printf("\t-o <dirname>\t- Output directory to write files to. CWD is changed *after* opening pst file\n");
    printf("\t-q\t- Quiet. Only print error messages\n");
    printf("\t-r\t- Recursive. Output in a recursive format\n");
    printf("\t-w\t- Overwrite any output mbox files\n");
    DEBUG_RET();
}


void version() {
    DEBUG_ENT("version");
    printf("ReadPST / LibPST v%s\n", VERSION);
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
    DEBUG_RET();
}


char *mk_kmail_dir(char *fname) {
    //change to that directory
    //make a directory based on OUTPUT_KMAIL_DIR_TEMPLATE
    //allocate space for OUTPUT_TEMPLATE and form a char* with fname
    //return that value
    char *dir, *out_name, *index;
    int x;
    DEBUG_ENT("mk_kmail_dir");
    if (kmail_chdir && chdir(kmail_chdir)) {
        x = errno;
        DIE(("mk_kmail_dir: Cannot change to directory %s: %s\n", kmail_chdir, strerror(x)));
    }
    dir = malloc(strlen(fname)+strlen(OUTPUT_KMAIL_DIR_TEMPLATE)+1);
    sprintf(dir, OUTPUT_KMAIL_DIR_TEMPLATE, fname);
    check_filename(dir);
    if (D_MKDIR(dir)) {
        //error occured
        if (errno != EEXIST) {
            x = errno;
            DIE(("mk_kmail_dir: Cannot create directory %s: %s\n", dir, strerror(x)));
        }
    }
    kmail_chdir = realloc(kmail_chdir, strlen(dir)+1);
    strcpy(kmail_chdir, dir);
    free (dir);

    //we should remove any existing indexes created by KMail, cause they might be different now
    index = malloc(strlen(fname)+strlen(KMAIL_INDEX)+1);
    sprintf(index, KMAIL_INDEX, fname);
    unlink(index);
    free(index);

    out_name = malloc(strlen(fname)+strlen(OUTPUT_TEMPLATE)+1);
    sprintf(out_name, OUTPUT_TEMPLATE, fname);
    DEBUG_RET();
    return out_name;
}


int close_kmail_dir() {
    // change ..
    int x;
    DEBUG_ENT("close_kmail_dir");
    if (kmail_chdir) { //only free kmail_chdir if not NULL. do not change directory
        free(kmail_chdir);
        kmail_chdir = NULL;
    } else {
        if (chdir("..")) {
            x = errno;
            DIE(("close_kmail_dir: Cannot move up dir (..): %s\n", strerror(x)));
        }
    }
    DEBUG_RET();
    return 0;
}


// this will create a directory by that name, then make an mbox file inside
// that dir.  any subsequent dirs will be created by name, and they will
// contain mbox files
char *mk_recurse_dir(char *dir) {
    int x;
    char *out_name;
    DEBUG_ENT("mk_recurse_dir");
    check_filename(dir);
    if (D_MKDIR (dir)) {
        if (errno != EEXIST) { // not an error because it exists
            x = errno;
            DIE(("mk_recurse_dir: Cannot create directory %s: %s\n", dir, strerror(x)));
        }
    }
    if (chdir (dir)) {
        x = errno;
        DIE(("mk_recurse_dir: Cannot change to directory %s: %s\n", dir, strerror(x)));
    }
    out_name = malloc(strlen("mbox")+1);
    strcpy(out_name, "mbox");
    DEBUG_RET();
    return out_name;
}


int close_recurse_dir() {
    int x;
    DEBUG_ENT("close_recurse_dir");
    if (chdir("..")) {
        x = errno;
        DIE(("close_recurse_dir: Cannot go up dir (..): %s\n", strerror(x)));
    }
    DEBUG_RET();
    return 0;
}


char *mk_separate_dir(char *dir) {
    size_t dirsize = strlen(dir) + 10;
    char dir_name[dirsize];
    int x = 0, y = 0;

    DEBUG_ENT("mk_separate_dir");
    do {
        if (y == 0)
            snprintf(dir_name, dirsize, "%s", dir);
        else
            snprintf(dir_name, dirsize, "%s" SEP_MAIL_FILE_TEMPLATE, dir, y); // enough for 9 digits allocated above

        check_filename(dir_name);
        DEBUG_MAIN(("about to try creating %s\n", dir_name));
        if (D_MKDIR(dir_name)) {
            if (errno != EEXIST) { // if there is an error, and it doesn't already exist
                x = errno;
                DIE(("mk_separate_dir: Cannot create directory %s: %s\n", dir, strerror(x)));
            }
        } else {
            break;
        }
        y++;
    } while (overwrite == 0);

    if (chdir(dir_name)) {
        x = errno;
        DIE(("mk_separate_dir: Cannot change to directory %s: %s\n", dir, strerror(x)));
    }

    if (overwrite) {
        // we should probably delete all files from this directory
#if !defined(WIN32) && !defined(__CYGWIN__)
        DIR * sdir = NULL;
        struct dirent *dirent = NULL;
        struct stat filestat;
        if (!(sdir = opendir("./"))) {
            WARN(("mk_separate_dir: Cannot open dir \"%s\" for deletion of old contents\n", "./"));
        } else {
            while ((dirent = readdir(sdir))) {
                if (lstat(dirent->d_name, &filestat) != -1)
                    if (S_ISREG(filestat.st_mode)) {
                        if (unlink(dirent->d_name)) {
                            y = errno;
                            DIE(("mk_separate_dir: unlink returned error on file %s: %s\n", dirent->d_name, strerror(y)));
                        }
                    }
            }
        }
#endif
    }

    // we don't return a filename here cause it isn't necessary.
    DEBUG_RET();
    return NULL;
}


int close_separate_dir() {
    int x;
    DEBUG_ENT("close_separate_dir");
    if (chdir("..")) {
        x = errno;
        DIE(("close_separate_dir: Cannot go up dir (..): %s\n", strerror(x)));
    }
    DEBUG_RET();
    return 0;
}


int mk_separate_file(struct file_ll *f) {
    const int name_offset = 1;
    DEBUG_ENT("mk_separate_file");
    DEBUG_MAIN(("opening next file to save email\n"));
    if (f->email_count > 999999999) { // bigger than nine 9's
        DIE(("mk_separate_file: The number of emails in this folder has become too high to handle"));
    }
    sprintf(f->name, SEP_MAIL_FILE_TEMPLATE, f->email_count + name_offset);
    if (f->output) fclose(f->output);
    f->output = NULL;
    check_filename(f->name);
    if (!(f->output = fopen(f->name, "w"))) {
        DIE(("mk_separate_file: Cannot open file to save email \"%s\"\n", f->name));
    }
    DEBUG_RET();
    return 0;
}


char *my_stristr(char *haystack, char *needle) {
    // my_stristr varies from strstr in that its searches are case-insensitive
    char *x=haystack, *y=needle, *z = NULL;
    if (!haystack || !needle) {
        return NULL;
    }
    while (*y != '\0' && *x != '\0') {
        if (tolower(*y) == tolower(*x)) {
            // move y on one
            y++;
            if (!z) {
                z = x; // store first position in haystack where a match is made
            }
        } else {
            y = needle; // reset y to the beginning of the needle
            z = NULL; // reset the haystack storage point
        }
        x++; // advance the search in the haystack
    }
    // If the haystack ended before our search finished, it's not a match.
    if (*y != '\0') return NULL;
    return z;
}


void check_filename(char *fname) {
    char *t = fname;
    DEBUG_ENT("check_filename");
    if (!t) {
        DEBUG_RET();
        return;
    }
    while ((t = strpbrk(t, "/\\:"))) {
        // while there are characters in the second string that we don't want
        *t = '_'; //replace them with an underscore
    }
    DEBUG_RET();
}


void write_separate_attachment(char f_name[], pst_item_attach* attach, int attach_num, pst_file* pst)
{
    FILE *fp = NULL;
    int x = 0;
    char *temp = NULL;

    // If there is a long filename (filename2) use that, otherwise
    // use the 8.3 filename (filename1)
    char *attach_filename = (attach->filename2) ? attach->filename2
                                                        : attach->filename1;
    DEBUG_ENT("write_separate_attachment");

    check_filename(f_name);
    if (!attach_filename) {
        // generate our own (dummy) filename for the attachement
        temp = xmalloc(strlen(f_name)+15);
        sprintf(temp, "%s-attach%i", f_name, attach_num);
    } else {
        // have an attachment name, make sure it's unique
        temp = xmalloc(strlen(f_name)+strlen(attach_filename)+15);
        do {
            if (fp) fclose(fp);
            if (x == 0)
                sprintf(temp, "%s-%s", f_name, attach_filename);
            else
                sprintf(temp, "%s-%s-%i", f_name, attach_filename, x);
        } while ((fp = fopen(temp, "r")) && ++x < 99999999);
        if (x > 99999999) {
            DIE(("error finding attachment name. exhausted possibilities to %s\n", temp));
        }
    }
    DEBUG_EMAIL(("Saving attachment to %s\n", temp));
    if (!(fp = fopen(temp, "w"))) {
        WARN(("write_separate_attachment: Cannot open attachment save file \"%s\"\n", temp));
    } else {
        if (attach->data)
            pst_fwrite(attach->data, 1, attach->size, fp);
        else {
            (void)pst_attach_to_file(pst, attach, fp);
        }
        fclose(fp);
    }
    if (temp) free(temp);
    DEBUG_RET();
}


void write_embedded_message(FILE* f_output, pst_item_attach* attach, char *boundary, pst_file* pf, char** extra_mime_headers)
{
    pst_index_ll *ptr;
    DEBUG_ENT("write_embedded_message");
    fprintf(f_output, "\n--%s\n", boundary);
    fprintf(f_output, "Content-Type: %s\n\n", attach->mimetype);
    ptr = pst_getID(pf, attach->id_val);
    pst_num_array *list = pst_parse_block(pf, ptr->id, NULL, NULL);
    if (list) {
        pst_item *item = (pst_item*) xmalloc(sizeof(pst_item));
        memset(item, 0, sizeof(pst_item));
        if (!pst_process(list, item, NULL)) {
            write_normal_email(f_output, "", item, MODE_NORMAL, 0, pf, 0, extra_mime_headers);
        }
        pst_freeItem(item);
        pst_free_list(list);
    }
    DEBUG_RET();
}


void write_inline_attachment(FILE* f_output, pst_item_attach* attach, char *boundary, pst_file* pst)
{
    char *attach_filename;
    char *enc = NULL; // base64 encoded attachment
    DEBUG_ENT("write_inline_attachment");
    DEBUG_EMAIL(("Attachment Size is %i\n", attach->size));
    DEBUG_EMAIL(("Attachment Pointer is %p\n", attach->data));
    if (attach->data) {
        enc = base64_encode (attach->data, attach->size);
        if (!enc) {
            DEBUG_EMAIL(("ERROR base64_encode returned NULL. Must have failed\n"));
            DEBUG_RET();
            return;
        }
    }

    fprintf(f_output, "\n--%s\n", boundary);
    if (!attach->mimetype) {
        fprintf(f_output, "Content-Type: %s\n", MIME_TYPE_DEFAULT);
    } else {
        fprintf(f_output, "Content-Type: %s\n", attach->mimetype);
    }
    fprintf(f_output, "Content-Transfer-Encoding: base64\n");

    // If there is a long filename (filename2) use that, otherwise
    // use the 8.3 filename (filename1)
    attach_filename = (attach->filename2) ? attach->filename2 : attach->filename1;
    if (!attach_filename) {
        fprintf(f_output, "Content-Disposition: inline\n\n");
    } else {
        fprintf(f_output, "Content-Disposition: attachment; filename=\"%s\"\n\n", attach_filename);
    }

    if (attach->data) {
        pst_fwrite(enc, 1, strlen(enc), f_output);
        DEBUG_EMAIL(("Attachment Size after encoding is %i\n", strlen(enc)));
        free(enc);  // caught by valgrind
    } else {
        (void)pst_attach_to_file_base64(pst, attach, f_output);
    }
    fprintf(f_output, "\n\n");
    DEBUG_RET();
}


void header_has_field(char *header, char *field, int *flag)
{
    DEBUG_ENT("header_has_field");
    if (my_stristr(header, field) || (strncasecmp(header, field+1, strlen(field)-1) == 0)) {
        DEBUG_EMAIL(("header block has %s header\n", field+1));
        *flag = 1;
    }
    DEBUG_RET();
}


void header_get_subfield(char *field, const char *subfield, char *body_subfield, size_t size_subfield)
{
    if (!field) return;
    DEBUG_ENT("header_get_subfield");
    char search[60];
    snprintf(search, sizeof(search), " %s=", subfield);
    field++;
    char *n = header_end_field(field);
    char *s = my_stristr(field, search);
    if (n && s && (s < n)) {
        char *e, *f, save;
        s += strlen(search);    // skip over subfield=
        if (*s == '"') {
            s++;
            e = strchr(s, '"');
        }
        else {
            e = strchr(s, ';');
            f = strchr(s, '\n');
            if (e && f && (f < e)) e = f;
        }
        if (!e || (e > n)) e = n;   // use the trailing lf as terminator if nothing better
        save = *e;
        *e = '\0';
            snprintf(body_subfield, size_subfield, "%s", s);  // copy the subfield to our buffer
        *e = save;
        DEBUG_EMAIL(("body %s %s from headers\n", subfield, body_subfield));
    }
    DEBUG_RET();
}

char* header_get_field(char *header, char *field)
{
    char *t = my_stristr(header, field);
    if (!t && (strncasecmp(header, field+1, strlen(field)-1) == 0)) t = header;
    return t;
}


// return pointer to \n at the end of this header field,
// or NULL if this field goes to the end of the string.
char *header_end_field(char *field)
{
    char *e = strchr(field+1, '\n');
    while (e && ((e[1] == ' ') || (e[1] == '\t'))) {
        e = strchr(e+1, '\n');
    }
    return e;
}


void header_strip_field(char *header, char *field)
{
    char *e;
    char *t = header_get_field(header, field);
    if (t) {
        char *e = header_end_field(t);
        if (e) {
            if (t == header) e++;   // if *t is not \n, we don't want to keep the \n at *e either.
            while (*e != '\0') {
                *t = *e;
                t++;
                e++;
            }
            *t = '\0';
        }
        else {
            // this was the last header field, truncate the headers
            *t = '\0';
        }
    }
}


int  test_base64(char *body)
{
    int b64 = 0;
    uint8_t *b = (uint8_t *)body;
    DEBUG_ENT("test_base64");
    while (*b != 0) {
        if ((*b < 32) && (*b != 9) && (*b != 10)) {
            DEBUG_EMAIL(("found base64 byte %d\n", (int)*b));
            DEBUG_HEXDUMPC(body, strlen(body), 0x10);
            b64 = 1;
            break;
        }
        b++;
    }
    DEBUG_RET();
    return b64;
}


void find_html_charset(char *html, char *charset, size_t charsetlen)
{
    const int  index = 1;
    const int nmatch = index+1;
    regmatch_t match[nmatch];
    DEBUG_ENT("find_html_charset");
    int rc = regexec(&meta_charset_pattern, html, nmatch, match, 0);
    if (rc == 0) {
        int s = match[index].rm_so;
        int e = match[index].rm_eo;
        if (s != -1) {
            char save = html[e];
            html[e] = '\0';
                snprintf(charset, charsetlen, "%s", html+s);    // copy the html charset
            html[e] = save;
            DEBUG_EMAIL(("charset %s from html text\n", charset));
        }
        else {
            DEBUG_EMAIL(("matching %d %d %d %d", match[0].rm_so, match[0].rm_eo, match[1].rm_so, match[1].rm_eo));
            DEBUG_HEXDUMPC(html, strlen(html), 0x10);
        }
    }
    else {
        DEBUG_EMAIL(("regexec returns %d\n", rc));
    }
    DEBUG_RET();
}


void find_rfc822_headers(char** extra_mime_headers)
{
    DEBUG_ENT("find_rfc822_headers");
    char *headers = *extra_mime_headers;
    if (headers) {
        char *temp, *t;
        while (temp = strstr(headers, "\n\n")) {
            temp[1] = '\0';
            t = header_get_field(headers, "\nContent-Type: ");
            if (t) {
                t++;
                DEBUG_EMAIL(("found content type header\n"));
                char *n = strchr(t, '\n');
                char *s = strstr(t, ": ");
                char *e = strchr(t, ';');
                if (!e || (e > n)) e = n;
                if (s && (s < e)) {
                    s += 2;
                    if (!strncasecmp(s, RFC822, e-s)) {
                        headers = temp+2;   // found rfc822 header
                        DEBUG_EMAIL(("found 822 headers\n%s\n", headers));
                        break;
                    }
                }
            }
            DEBUG_EMAIL(("skipping to next block after\n%s\n", headers));
            headers = temp+2;   // skip to next chunk of headers
        }
        *extra_mime_headers = headers;
    }
    DEBUG_RET();
}


void write_body_part(FILE* f_output, char *body, char *mime, char *charset, char *boundary)
{
    char *needfree = NULL;
    DEBUG_ENT("write_body_part");
    if (strcasecmp("utf-8", charset)) {
        // try to convert to the specified charset since it is not utf-8
        size_t rc;
        DEBUG_EMAIL(("Convert %s utf-8 to %s\n", mime, charset));
        vbuf *newer = vballoc(2);
        rc = vb_utf8to8bit(newer, body, strlen(body) + 1, charset);
        if (rc == (size_t)-1) {
            // unable to convert, maybe it is already in that character set
            free(newer->b);
            DEBUG_EMAIL(("Failed to convert %s utf-8 to %s\n", mime, charset));
        }
        else {
            needfree = body = newer->b;
        }
        free(newer);
    }
    removeCR(body);
    int base64 = test_base64(body);
    fprintf(f_output, "\n--%s\n", boundary);
    fprintf(f_output, "Content-Type: %s; charset=\"%s\"\n", mime, charset);
    if (base64) fprintf(f_output, "Content-Transfer-Encoding: base64\n");
    fprintf(f_output, "\n");
    if (base64) {
        char *enc = base64_encode(body, strlen(body));
        if (enc) {
            write_email_body(f_output, enc);
            fprintf(f_output, "\n");
            free(enc);
        }
    }
    else {
        write_email_body(f_output, body);
    }
    if (needfree) free(needfree);
    DEBUG_RET();
}


void write_normal_email(FILE* f_output, char f_name[], pst_item* item, int mode, int mode_MH, pst_file* pst, int save_rtf, char** extra_mime_headers)
{
    char boundary[60];
    char body_charset[60];
    char body_report[60];
    char sender[60];
    int  sender_known = 0;
    char *temp = NULL;
    int attach_num;
    time_t em_time;
    char *c_time;
    char *headers = (item->email->header) ? item->email->header : *extra_mime_headers;
    int has_from, has_subject, has_to, has_cc, has_date, has_msgid;
    has_from = has_subject = has_to = has_cc = has_date = has_msgid = 0;
    DEBUG_ENT("write_normal_email");

    // setup default body character set and report type
    snprintf(body_charset, sizeof(body_charset), "%s", (item->email->body_charset) ? item->email->body_charset : "utf-8");
    body_report[0] = '\0';

    // setup default sender
    if (item->email->sender_address && strchr(item->email->sender_address, '@')) {
        temp = item->email->sender_address;
        sender_known = 1;
    }
    else {
        temp = "MAILER-DAEMON";
    }
    snprintf(sender, sizeof(sender), "%s", temp);

    // convert the sent date if it exists, or set it to a fixed date
    if (item->email->sent_date) {
        em_time = fileTimeToUnixTime(item->email->sent_date, 0);
        c_time = ctime(&em_time);
        if (c_time)
            c_time[strlen(c_time)-1] = '\0'; //remove end \n
        else
            c_time = "Fri Dec 28 12:06:21 2001";
    } else
        c_time= "Fri Dec 28 12:06:21 2001";

    // create our MIME boundary here.
    snprintf(boundary, sizeof(boundary), "--boundary-LibPST-iamunique-%i_-_-", rand());

    // we will always look at the headers to discover some stuff
    if (headers ) {
        char *t;
        removeCR(headers);

        temp = strstr(headers, "\n\n");
        if (temp) {
            // cut off our real rfc822 headers here
            temp[1] = '\0';
            // pointer to all the embedded MIME headers.
            // we use these to find the actual rfc822 headers for embedded message/rfc822 mime parts
            *extra_mime_headers = temp+2;
            DEBUG_EMAIL(("Found extra mime headers\n%s\n", temp+2));
        }

        // Check if the headers have all the necessary fields
        header_has_field(headers, "\nFrom: ",        &has_from);
        header_has_field(headers, "\nTo: ",          &has_to);
        header_has_field(headers, "\nSubject: ",     &has_subject);
        header_has_field(headers, "\nDate: ",        &has_date);
        header_has_field(headers, "\nCC: ",          &has_cc);
        header_has_field(headers, "\nMessage-Id: ",  &has_msgid);

        // look for charset and report-type in Content-Type header
        t = header_get_field(headers, "\nContent-Type: ");
        header_get_subfield(t, "charset", body_charset, sizeof(body_charset));
        header_get_subfield(t, "report-type", body_report, sizeof(body_report));

        // derive a proper sender email address
        if (!sender_known) {
            t = header_get_field(headers, "\nFrom: ");
            if (t) {
                // assume address is on the first line, rather than on a continuation line
                t++;
                char *n = strchr(t, '\n');
                char *s = strchr(t, '<');
                char *e = strchr(t, '>');
                if (s && e && n && (s < e) && (e < n)) {
                char save = *e;
                *e = '\0';
                    snprintf(sender, sizeof(sender), "%s", s+1);
                *e = save;
                }
            }
        }

        // Strip out the mime headers and some others that we don't want to emit
        header_strip_field(headers, "\nMicrosoft Mail Internet Headers");
        header_strip_field(headers, "\nMIME-Version: ");
        header_strip_field(headers, "\nContent-Type: ");
        header_strip_field(headers, "\nContent-Transfer-Encoding: ");
        header_strip_field(headers, "\nContent-class: ");
        header_strip_field(headers, "\nX-MimeOLE: ");
        header_strip_field(headers, "\nBcc:");
        header_strip_field(headers, "\nX-From_: ");
    }

    DEBUG_EMAIL(("About to print Header\n"));

    if (item && item->email && item->email->subject && item->email->subject->subj) {
        DEBUG_EMAIL(("item->email->subject->subj = %s\n", item->email->subject->subj));
    }

    if (mode != MODE_SEPARATE) {
        // most modes need this separator line
        fprintf(f_output, "From %s %s\n", sender, c_time);
    }

    // print the supplied email headers
    if (headers) {
        int len;
        fprintf(f_output, "%s", headers);
        // make sure the headers end with a \n
        len = strlen(headers);
        if (!len || (headers[len-1] != '\n')) fprintf(f_output, "\n");
    }

    // create required header fields that are not already written

    if (!has_from) {
        fprintf(f_output, "From: \"%s\" <%s>\n", item->email->outlook_sender_name, sender);
    }

    if (!has_subject) {
        if (item->email->subject && item->email->subject->subj) {
            fprintf(f_output, "Subject: %s\n", item->email->subject->subj);
        } else {
            fprintf(f_output, "Subject: \n");
        }
    }

    if (!has_to && item->email->sentto_address) {
        fprintf(f_output, "To: %s\n", item->email->sentto_address);
    }

    if (!has_cc && item->email->cc_address) {
        fprintf(f_output, "Cc: %s\n", item->email->cc_address);
    }

    if (!has_date && item->email->sent_date) {
        char c_time[C_TIME_SIZE];
        strftime(c_time, C_TIME_SIZE, "%a, %d %b %Y %H:%M:%S %z", gmtime(&em_time));
        fprintf(f_output, "Date: %s\n", c_time);
    }

    if (!has_msgid && item->email->messageid) {
        fprintf(f_output, "Message-Id: %s\n", item->email->messageid);
    }

    // add forensic headers to capture some .pst stuff that is not really
    // needed or used by mail clients
    if (item->email->sender_address && !strchr(item->email->sender_address, '@')
                                    && strcmp(item->email->sender_address, ".")) {
        fprintf(f_output, "X-libpst-forensic-sender: %s\n", item->email->sender_address);
    }

    if (item->email->bcc_address) {
        fprintf(f_output, "X-libpst-forensic-bcc: %s\n", item->email->bcc_address);
    }

    // add our own mime headers
    fprintf(f_output, "MIME-Version: 1.0\n");
    if (body_report[0] != '\0') {
        // multipart/report for DSN/MDN reports
        fprintf(f_output, "Content-Type: multipart/report; report-type=%s;\n\tboundary=\"%s\"\n", body_report, boundary);
    }
    else if (item->attach || (item->email->rtf_compressed && save_rtf)
                          || item->email->encrypted_body
                          || item->email->encrypted_htmlbody) {
        // use multipart/mixed if we have attachments
        fprintf(f_output, "Content-Type: multipart/mixed;\n\tboundary=\"%s\"\n", boundary);
    } else {
        // else use multipart/alternative
        fprintf(f_output, "Content-Type: multipart/alternative;\n\tboundary=\"%s\"\n", boundary);
    }
    fprintf(f_output, "\n");    // end of headers, start of body

    // now dump the body parts
    if (item->email->body) {
        write_body_part(f_output, item->email->body, "text/plain", body_charset, boundary);
    }

    if (item->email->htmlbody) {
        find_html_charset(item->email->htmlbody, body_charset, sizeof(body_charset));
        write_body_part(f_output, item->email->htmlbody, "text/html", body_charset, boundary);
    }

    if (item->email->rtf_compressed && save_rtf) {
        pst_item_attach* attach = (pst_item_attach*)xmalloc(sizeof(pst_item_attach));
        DEBUG_EMAIL(("Adding RTF body as attachment\n"));
        memset(attach, 0, sizeof(pst_item_attach));
        attach->next = item->attach;
        item->attach = attach;
        attach->data = lzfu_decompress(item->email->rtf_compressed, item->email->rtf_compressed_size, &attach->size);
        attach->filename2 = strdup(RTF_ATTACH_NAME);
        attach->mimetype  = strdup(RTF_ATTACH_TYPE);
    }

    if (item->email->encrypted_body || item->email->encrypted_htmlbody) {
        // if either the body or htmlbody is encrypted, add them as attachments
        if (item->email->encrypted_body) {
            pst_item_attach* attach = (pst_item_attach*)xmalloc(sizeof(pst_item_attach));
            DEBUG_EMAIL(("Adding Encrypted Body as attachment\n"));
            attach = (pst_item_attach*) xmalloc(sizeof(pst_item_attach));
            memset(attach, 0, sizeof(pst_item_attach));
            attach->next = item->attach;
            item->attach = attach;
            attach->data = item->email->encrypted_body;
            attach->size = item->email->encrypted_body_size;
            item->email->encrypted_body = NULL;
        }

        if (item->email->encrypted_htmlbody) {
            pst_item_attach* attach = (pst_item_attach*)xmalloc(sizeof(pst_item_attach));
            DEBUG_EMAIL(("Adding encrypted HTML body as attachment\n"));
            attach = (pst_item_attach*) xmalloc(sizeof(pst_item_attach));
            memset(attach, 0, sizeof(pst_item_attach));
            attach->next = item->attach;
            item->attach = attach;
            attach->data = item->email->encrypted_htmlbody;
            attach->size = item->email->encrypted_htmlbody_size;
            item->email->encrypted_htmlbody = NULL;
        }
        write_email_body(f_output, "The body of this email is encrypted. This isn't supported yet, but the body is now an attachment\n");
    }

    // other attachments
    {
        pst_item_attach* attach;
        attach_num = 0;
        for (attach = item->attach; attach; attach = attach->next) {
            DEBUG_EMAIL(("Attempting Attachment encoding\n"));
            if (!attach->data && attach->mimetype && !strcmp(attach->mimetype, RFC822)) {
                DEBUG_EMAIL(("seem to have special embedded message attachment\n"));
                find_rfc822_headers(extra_mime_headers);
                write_embedded_message(f_output, attach, boundary, pst, extra_mime_headers);
            }
            else if (mode == MODE_SEPARATE && !mode_MH)
                write_separate_attachment(f_name, attach, ++attach_num, pst);
            else
                write_inline_attachment(f_output, attach, boundary, pst);
        }
    }

    // end of this mail message
    if (mode != MODE_SEPARATE) { /* do not add a boundary after the last attachment for mode_MH */
        DEBUG_EMAIL(("Writing buffer between emails\n"));
        fprintf(f_output, "\n--%s--\n", boundary);
        fprintf(f_output, "\n\n");
    }
    DEBUG_RET();
}


void write_vcard(FILE* f_output, pst_item_contact* contact, char comment[])
{
    // We can only call rfc escape once per printf, since the second call
    // may free the buffer returned by the first call.
    // I had tried to place those into a single printf - Carl.

    DEBUG_ENT("write_vcard");
    // the specification I am following is (hopefully) RFC2426 vCard Mime Directory Profile
    fprintf(f_output, "BEGIN:VCARD\n");
    fprintf(f_output, "FN:%s\n", pst_rfc2426_escape(contact->fullname));

    //fprintf(f_output, "N:%s;%s;%s;%s;%s\n",
    fprintf(f_output, "N:%s;", (!contact->surname)             ? "" : pst_rfc2426_escape(contact->surname));
    fprintf(f_output, "%s;",   (!contact->first_name)          ? "" : pst_rfc2426_escape(contact->first_name));
    fprintf(f_output, "%s;",   (!contact->middle_name)         ? "" : pst_rfc2426_escape(contact->middle_name));
    fprintf(f_output, "%s;",   (!contact->display_name_prefix) ? "" : pst_rfc2426_escape(contact->display_name_prefix));
    fprintf(f_output, "%s\n",  (!contact->suffix)              ? "" : pst_rfc2426_escape(contact->suffix));

    if (contact->nickname)
        fprintf(f_output, "NICKNAME:%s\n", pst_rfc2426_escape(contact->nickname));
    if (contact->address1)
        fprintf(f_output, "EMAIL:%s\n", pst_rfc2426_escape(contact->address1));
    if (contact->address2)
        fprintf(f_output, "EMAIL:%s\n", pst_rfc2426_escape(contact->address2));
    if (contact->address3)
        fprintf(f_output, "EMAIL:%s\n", pst_rfc2426_escape(contact->address3));
    if (contact->birthday)
        fprintf(f_output, "BDAY:%s\n", pst_rfc2425_datetime_format(contact->birthday));

    if (contact->home_address) {
        //fprintf(f_output, "ADR;TYPE=home:%s;%s;%s;%s;%s;%s;%s\n",
        fprintf(f_output, "ADR;TYPE=home:%s;",  (!contact->home_po_box)      ? "" : pst_rfc2426_escape(contact->home_po_box));
        fprintf(f_output, "%s;",                ""); // extended Address
        fprintf(f_output, "%s;",                (!contact->home_street)      ? "" : pst_rfc2426_escape(contact->home_street));
        fprintf(f_output, "%s;",                (!contact->home_city)        ? "" : pst_rfc2426_escape(contact->home_city));
        fprintf(f_output, "%s;",                (!contact->home_state)       ? "" : pst_rfc2426_escape(contact->home_state));
        fprintf(f_output, "%s;",                (!contact->home_postal_code) ? "" : pst_rfc2426_escape(contact->home_postal_code));
        fprintf(f_output, "%s\n",               (!contact->home_country)     ? "" : pst_rfc2426_escape(contact->home_country));
        fprintf(f_output, "LABEL;TYPE=home:%s\n", pst_rfc2426_escape(contact->home_address));
    }

    if (contact->business_address) {
        //fprintf(f_output, "ADR;TYPE=work:%s;%s;%s;%s;%s;%s;%s\n",
        fprintf(f_output, "ADR;TYPE=work:%s;",  (!contact->business_po_box)      ? "" : pst_rfc2426_escape(contact->business_po_box));
        fprintf(f_output, "%s;",                ""); // extended Address
        fprintf(f_output, "%s;",                (!contact->business_street)      ? "" : pst_rfc2426_escape(contact->business_street));
        fprintf(f_output, "%s;",                (!contact->business_city)        ? "" : pst_rfc2426_escape(contact->business_city));
        fprintf(f_output, "%s;",                (!contact->business_state)       ? "" : pst_rfc2426_escape(contact->business_state));
        fprintf(f_output, "%s;",                (!contact->business_postal_code) ? "" : pst_rfc2426_escape(contact->business_postal_code));
        fprintf(f_output, "%s\n",               (!contact->business_country)     ? "" : pst_rfc2426_escape(contact->business_country));
        fprintf(f_output, "LABEL;TYPE=work:%s\n", pst_rfc2426_escape(contact->business_address));
    }

    if (contact->other_address) {
        //fprintf(f_output, "ADR;TYPE=postal:%s;%s;%s;%s;%s;%s;%s\n",
        fprintf(f_output, "ADR;TYPE=postal:%s;",(!contact->other_po_box)       ? "" : pst_rfc2426_escape(contact->other_po_box));
        fprintf(f_output, "%s;",                ""); // extended Address
        fprintf(f_output, "%s;",                (!contact->other_street)       ? "" : pst_rfc2426_escape(contact->other_street));
        fprintf(f_output, "%s;",                (!contact->other_city)         ? "" : pst_rfc2426_escape(contact->other_city));
        fprintf(f_output, "%s;",                (!contact->other_state)        ? "" : pst_rfc2426_escape(contact->other_state));
        fprintf(f_output, "%s;",                (!contact->other_postal_code)  ? "" : pst_rfc2426_escape(contact->other_postal_code));
        fprintf(f_output, "%s\n",               (!contact->other_country)      ? "" : pst_rfc2426_escape(contact->other_country));
        fprintf(f_output, "LABEL;TYPE=postal:%s\n", pst_rfc2426_escape(contact->other_address));
    }

    if (contact->business_fax)      fprintf(f_output, "TEL;TYPE=work,fax:%s\n",         pst_rfc2426_escape(contact->business_fax));
    if (contact->business_phone)    fprintf(f_output, "TEL;TYPE=work,voice:%s\n",       pst_rfc2426_escape(contact->business_phone));
    if (contact->business_phone2)   fprintf(f_output, "TEL;TYPE=work,voice:%s\n",       pst_rfc2426_escape(contact->business_phone2));
    if (contact->car_phone)         fprintf(f_output, "TEL;TYPE=car,voice:%s\n",        pst_rfc2426_escape(contact->car_phone));
    if (contact->home_fax)          fprintf(f_output, "TEL;TYPE=home,fax:%s\n",         pst_rfc2426_escape(contact->home_fax));
    if (contact->home_phone)        fprintf(f_output, "TEL;TYPE=home,voice:%s\n",       pst_rfc2426_escape(contact->home_phone));
    if (contact->home_phone2)       fprintf(f_output, "TEL;TYPE=home,voice:%s\n",       pst_rfc2426_escape(contact->home_phone2));
    if (contact->isdn_phone)        fprintf(f_output, "TEL;TYPE=isdn:%s\n",             pst_rfc2426_escape(contact->isdn_phone));
    if (contact->mobile_phone)      fprintf(f_output, "TEL;TYPE=cell,voice:%s\n",       pst_rfc2426_escape(contact->mobile_phone));
    if (contact->other_phone)       fprintf(f_output, "TEL;TYPE=msg:%s\n",              pst_rfc2426_escape(contact->other_phone));
    if (contact->pager_phone)       fprintf(f_output, "TEL;TYPE=pager:%s\n",            pst_rfc2426_escape(contact->pager_phone));
    if (contact->primary_fax)       fprintf(f_output, "TEL;TYPE=fax,pref:%s\n",         pst_rfc2426_escape(contact->primary_fax));
    if (contact->primary_phone)     fprintf(f_output, "TEL;TYPE=phone,pref:%s\n",       pst_rfc2426_escape(contact->primary_phone));
    if (contact->radio_phone)       fprintf(f_output, "TEL;TYPE=pcs:%s\n",              pst_rfc2426_escape(contact->radio_phone));
    if (contact->telex)             fprintf(f_output, "TEL;TYPE=bbs:%s\n",              pst_rfc2426_escape(contact->telex));
    if (contact->job_title)         fprintf(f_output, "TITLE:%s\n",                     pst_rfc2426_escape(contact->job_title));
    if (contact->profession)        fprintf(f_output, "ROLE:%s\n",                      pst_rfc2426_escape(contact->profession));
    if (contact->assistant_name || contact->assistant_phone) {
        fprintf(f_output, "AGENT:BEGIN:VCARD\n");
        if (contact->assistant_name)    fprintf(f_output, "FN:%s\n",                    pst_rfc2426_escape(contact->assistant_name));
        if (contact->assistant_phone)   fprintf(f_output, "TEL:%s\n",                   pst_rfc2426_escape(contact->assistant_phone));
    }
    if (contact->company_name)      fprintf(f_output, "ORG:%s\n",                       pst_rfc2426_escape(contact->company_name));
    if (comment)                    fprintf(f_output, "NOTE:%s\n",                      pst_rfc2426_escape(comment));

    fprintf(f_output, "VERSION: 3.0\n");
    fprintf(f_output, "END:VCARD\n\n");
    DEBUG_RET();
}


void write_appointment(FILE* f_output, pst_item_appointment* appointment,
               pst_item_email* email, FILETIME* create_date, FILETIME* modify_date)
{
    fprintf(f_output, "BEGIN:VEVENT\n");
    if (create_date)
        fprintf(f_output, "CREATED:%s\n",
            pst_rfc2445_datetime_format(create_date));
    if (modify_date)
        fprintf(f_output, "LAST-MOD:%s\n",
            pst_rfc2445_datetime_format(modify_date));
    if (email && email->subject)
        fprintf(f_output, "SUMMARY:%s\n",
            pst_rfc2426_escape(email->subject->subj));
    if (email && email->body)
        fprintf(f_output, "DESCRIPTION:%s\n",
            pst_rfc2426_escape(email->body));
    if (appointment && appointment->start)
        fprintf(f_output, "DTSTART;VALUE=DATE-TIME:%s\n",
            pst_rfc2445_datetime_format(appointment->start));
    if (appointment && appointment->end)
        fprintf(f_output, "DTEND;VALUE=DATE-TIME:%s\n",
            pst_rfc2445_datetime_format(appointment->end));
    if (appointment && appointment->location)
        fprintf(f_output, "LOCATION:%s\n",
            pst_rfc2426_escape(appointment->location));
    if (appointment) {
        switch (appointment->showas) {
            case PST_FREEBUSY_TENTATIVE:
                fprintf(f_output, "STATUS:TENTATIVE\n");
                break;
            case PST_FREEBUSY_FREE:
                // mark as transparent and as confirmed
                fprintf(f_output, "TRANSP:TRANSPARENT\n");
            case PST_FREEBUSY_BUSY:
            case PST_FREEBUSY_OUT_OF_OFFICE:
                fprintf(f_output, "STATUS:CONFIRMED\n");
                break;
        }
        switch (appointment->label) {
            case PST_APP_LABEL_NONE:
                fprintf(f_output, "CATEGORIES:NONE\n");
                break;
            case PST_APP_LABEL_IMPORTANT:
                fprintf(f_output, "CATEGORIES:IMPORTANT\n");
                break;
            case PST_APP_LABEL_BUSINESS:
                fprintf(f_output, "CATEGORIES:BUSINESS\n");
                break;
            case PST_APP_LABEL_PERSONAL:
                fprintf(f_output, "CATEGORIES:PERSONAL\n");
                break;
            case PST_APP_LABEL_VACATION:
                fprintf(f_output, "CATEGORIES:VACATION\n");
                break;
            case PST_APP_LABEL_MUST_ATTEND:
                fprintf(f_output, "CATEGORIES:MUST-ATTEND\n");
                break;
            case PST_APP_LABEL_TRAVEL_REQ:
                fprintf(f_output, "CATEGORIES:TRAVEL-REQUIRED\n");
                break;
            case PST_APP_LABEL_NEEDS_PREP:
                fprintf(f_output, "CATEGORIES:NEEDS-PREPARATION\n");
                break;
            case PST_APP_LABEL_BIRTHDAY:
                fprintf(f_output, "CATEGORIES:BIRTHDAY\n");
                break;
            case PST_APP_LABEL_ANNIVERSARY:
                fprintf(f_output, "CATEGORIES:ANNIVERSARY\n");
                break;
            case PST_APP_LABEL_PHONE_CALL:
                fprintf(f_output, "CATEGORIES:PHONE-CALL\n");
                break;
        }
    }
    fprintf(f_output, "END:VEVENT\n\n");
}


void create_enter_dir(struct file_ll* f, pst_item *item)
{
    f->email_count  = 0;
    f->skip_count   = 0;
    f->type         = item->type;
    f->stored_count = (item->folder) ? item->folder->email_count : 0;

    DEBUG_ENT("create_enter_dir");
    if (mode == MODE_KMAIL)
        f->name = mk_kmail_dir(item->file_as); //create directory and form filename
    else if (mode == MODE_RECURSE)
        f->name = mk_recurse_dir(item->file_as);
    else if (mode == MODE_SEPARATE) {
        // do similar stuff to recurse here.
        mk_separate_dir(item->file_as);
        f->name = (char*) xmalloc(10);
        memset(f->name, 0, 10);
        //      sprintf(f->name, SEP_MAIL_FILE_TEMPLATE, f->email_count);
    } else {
        f->name = (char*) xmalloc(strlen(item->file_as)+strlen(OUTPUT_TEMPLATE)+1);
        sprintf(f->name, OUTPUT_TEMPLATE, item->file_as);
    }

    f->dname = (char*) xmalloc(strlen(item->file_as)+1);
    strcpy(f->dname, item->file_as);

    if (overwrite != 1) {
        int x = 0;
        char *temp = (char*) xmalloc (strlen(f->name)+10); //enough room for 10 digits

        sprintf(temp, "%s", f->name);
        check_filename(temp);
        while ((f->output = fopen(temp, "r"))) {
            DEBUG_MAIN(("need to increase filename because one already exists with that name\n"));
            DEBUG_MAIN(("- increasing it to %s%d\n", f->name, x));
            x++;
            sprintf(temp, "%s%08d", f->name, x);
            DEBUG_MAIN(("- trying \"%s\"\n", f->name));
            if (x == 99999999) {
                DIE(("create_enter_dir: Why can I not create a folder %s? I have tried %i extensions...\n", f->name, x));
            }
            fclose(f->output);
        }
        if (x > 0) { //then the f->name should change
            free (f->name);
            f->name = temp;
        } else {
            free(temp);
        }
    }

    DEBUG_MAIN(("f->name = %s\nitem->folder_name = %s\n", f->name, item->file_as));
    if (mode != MODE_SEPARATE) {
        check_filename(f->name);
        if (!(f->output = fopen(f->name, "w"))) {
            DIE(("create_enter_dir: Could not open file \"%s\" for write\n", f->name));
        }
    }
    DEBUG_RET();
}


void close_enter_dir(struct file_ll *f)
{
    DEBUG_MAIN(("main: Email Count for folder %s is %i\n", f->dname, f->email_count));
    if (output_mode != OUTPUT_QUIET)
        printf("\t\"%s\" - %i items done, skipped %i, should have been %i\n",
               f->dname, f->email_count, f->skip_count, f->stored_count);
    if (f->output) fclose(f->output);
    free(f->name);
    free(f->dname);

    if (mode == MODE_KMAIL)
        close_kmail_dir();
    else if (mode == MODE_RECURSE)
        close_recurse_dir();
    else if (mode == MODE_SEPARATE)
        close_separate_dir();
}

