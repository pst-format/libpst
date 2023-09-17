/***
 * readpst.c
 * Part of the LibPST project
 * Written by David Smith
 *            dave.s@earthcorp.com
 */

#include "define.h"
#include "lzfu.h"
#include "msg.h"

#define OUTPUT_TEMPLATE "%s.%s"
#define OUTPUT_KMAIL_DIR_TEMPLATE ".%s.directory"
#define KMAIL_INDEX "../.%s.index"

// max size of the c_time char*. It will store the date of the email
#define C_TIME_SIZE 500

struct file_ll {
    char *name[PST_TYPE_MAX];
    char *dname;
    FILE * output[PST_TYPE_MAX];
    int32_t stored_count;
    int32_t item_count;
    int32_t skip_count;
};

int       grim_reaper();
pid_t     try_fork(char* folder);
void      process(pst_item *outeritem, pst_desc_tree *d_ptr);
void      write_email_body(FILE *f, char *body);
void      removeCR(char *c);
void      usage();
void      version();
void      mk_kmail_dir(char* fname);
int       close_kmail_dir();
void      mk_recurse_dir(char* dir);
int       close_recurse_dir();
void      mk_separate_dir(char *dir);
int       close_separate_dir();
void      mk_separate_file(struct file_ll *f, int32_t t, char *extension, int openit);
void      close_separate_file(struct file_ll *f);
char*     my_stristr(char *haystack, char *needle);
void      check_filename(char *fname);
int       acceptable_ext(pst_item_attach* attach);
void      write_separate_attachment(char f_name[], pst_item_attach* attach, int attach_num, pst_file* pst);
void      write_embedded_message(FILE* f_output, pst_item_attach* attach, char *boundary, pst_file* pf, int save_rtf, char** extra_mime_headers);
void      write_inline_attachment(FILE* f_output, pst_item_attach* attach, char *boundary, pst_file* pst);
int       valid_headers(char *header);
void      header_has_field(char *header, char *field, int *flag);
void      header_get_subfield(char *field, const char *subfield, char *body_subfield, size_t size_subfield);
char*     header_get_field(char *header, char *field);
char*     header_end_field(char *field);
void      header_strip_field(char *header, char *field);
int       test_base64(char *body, size_t len);
void      find_html_charset(char *html, char *charset, size_t charsetlen);
void      find_rfc822_headers(char** extra_mime_headers);
void      write_body_part(FILE* f_output, pst_string *body, char *mime, char *charset, char *boundary, pst_file* pst);
void      write_schedule_part_data(FILE* f_output, pst_item* item, const char* sender, const char* method);
void      write_schedule_part(FILE* f_output, pst_item* item, const char* sender, const char* boundary);
void      write_normal_email(FILE* f_output, char f_name[], pst_item* item, int current_mode, int mode_MH, pst_file* pst, int save_rtf, int embedding, char** extra_mime_headers);
void      write_vcard(FILE* f_output, pst_item *item, pst_item_contact* contact, char comment[]);
int       write_extra_categories(FILE* f_output, pst_item* item);
void      write_journal(FILE* f_output, pst_item* item);
void      write_appointment(FILE* f_output, pst_item *item);
void      create_enter_dir(struct file_ll* f, pst_item *item);
void      close_enter_dir(struct file_ll *f);
char*     quote_string(char *inp);

const char*  prog_name;
char*  output_dir = ".";

// Normal mode just creates mbox format files in the current directory. Each file is named
// the same as the folder's name that it represents
#define MODE_NORMAL 0

// KMail mode creates a directory structure suitable for being used directly
// by the KMail application
#define MODE_KMAIL 1

// recurse mode creates a directory structure like the PST file. Each directory
// contains only one file which stores the emails in mboxrd format.
#define MODE_RECURSE 2

// separate mode creates the same directory structure as recurse. The emails are stored in
// separate files, numbering from 1 upward. Attachments belonging to the emails are
// saved as email_no-filename (e.g. 1-samplefile.doc or 1-Attachment2.zip)
#define MODE_SEPARATE 3


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

// Output type mode flags
#define OTMODE_EMAIL        1
#define OTMODE_APPOINTMENT  2
#define OTMODE_JOURNAL      4
#define OTMODE_CONTACT      8

// output settings for RTF bodies
// filename for the attachment
#define RTF_ATTACH_NAME "rtf-body.rtf"
// mime type for the attachment
#define RTF_ATTACH_TYPE "application/rtf"

// global settings
int         mode         = MODE_NORMAL;
int         mode_MH      = 0;   // a submode of MODE_SEPARATE
int         mode_EX      = 0;   // a submode of MODE_SEPARATE
int         mode_MSG     = 0;   // a submode of MODE_SEPARATE
int         mode_thunder = 0;   // a submode of MODE_RECURSE
int         output_mode  = OUTPUT_NORMAL;
int         contact_mode = CMODE_VCARD;
int         deleted_mode = DMODE_EXCLUDE;
int         output_type_mode = 0xff;    // Default to all.
int         contact_mode_specified = 0;
int         overwrite = 0;
int         prefer_utf8 = 0;
int         save_rtf_body = 1;
int         file_name_len = 10;     // enough room for MODE_SPEARATE file name
pst_file    pstfile;
regex_t     meta_charset_pattern;
char*       default_charset = NULL;
char*       acceptable_extensions = NULL;

int         number_processors = 1;  // number of cpus we have
int         max_children  = 0;      // based on number of cpus and command line args
int         max_child_specified = 0;// have command line arg -j
int         active_children;        // number of children of this process, cannot be larger than max_children
pid_t*      child_processes;        // setup by main(), and at the start of new child process

#ifdef HAVE_SEMAPHORE_H
int         shared_memory_id;
sem_t*      global_children = NULL;
sem_t*      output_mutex    = NULL;
#endif


int grim_reaper(int waitall)
{
    int available = 0;
#ifdef HAVE_FORK
#ifdef HAVE_SEMAPHORE_H
    if (global_children) {
        //sem_getvalue(global_children, &available);
        //printf("grim reaper %s for pid %d (parent %d) with %d children, %d available\n", (waitall) ? "all" : "", getpid(), getppid(), active_children, available);
        //fflush(stdout);
        int i,j;
        for (i=0; i<active_children; i++) {
            int status;
            pid_t child = child_processes[i];
            pid_t ch = waitpid(child, &status, ((waitall) ? 0 : WNOHANG));
            if (ch == child) {
                // check termination status
                //if (WIFEXITED(status)) {
                //    int ext = WEXITSTATUS(status);
                //    printf("Process %d exited with status  %d\n", child, ext);
                //    fflush(stdout);
                //}
                if (WIFSIGNALED(status)) {
                    int sig = WTERMSIG(status);
                    DEBUG_INFO(("Process %d terminated with signal %d\n", child, sig));
                    //printf("Process %d terminated with signal %d\n", child, sig);
                    //fflush(stdout);
                }
                if (status != 0) {
                    exit(status);
                }
                // this has terminated, remove it from the list
                for (j=i; j<active_children-1; j++) {
                    child_processes[j] = child_processes[j+1];
                }
                active_children--;
                i--;
            }
        }
        sem_getvalue(global_children, &available);
        //printf("grim reaper %s for pid %d with %d children, %d available\n", (waitall) ? "all" : "", getpid(), active_children, available);
        //fflush(stdout);
    }
#endif
#endif
    return available;
}


pid_t try_fork(char *folder)
{
#ifdef HAVE_FORK
#ifdef HAVE_SEMAPHORE_H
    int available = grim_reaper(0);
    // If children have called sem_post but not exited yet, we could have available > 0 but active_children == max_children
    if (available && active_children < max_children) {
        sem_wait(global_children);
        pid_t child = fork();
        if (child < 0) {
            // fork failed, pretend it worked and we are the child
            return 0;
        }
        else if (child == 0) {
            // fork worked, and we are the child, reinitialize *our* list of children
            active_children = 0;
            memset(child_processes, 0, sizeof(pid_t) * max_children);
            pst_reopen(&pstfile);   // close and reopen the pst file to get an independent file position pointer
        }
        else {
            // fork worked, and we are the parent, record this child that we need to wait for
            //pid_t me = getpid();
            //printf("parent %d forked child pid %d to process folder %s\n", me, child, folder);
            //fflush(stdout);
            child_processes[active_children++] = child;
        }
        return child;
    }
    else {
        return 0;   // pretend to have forked and we are the child
    }
#endif
#endif
    return 0;
}


void process(pst_item *outeritem, pst_desc_tree *d_ptr)
{
    struct file_ll ff;
    pst_item *item = NULL;

    DEBUG_ENT("process");
    create_enter_dir(&ff, outeritem);

    for (; d_ptr; d_ptr = d_ptr->next) {
        DEBUG_INFO(("New item record\n"));
        if (!d_ptr->desc) {
            ff.skip_count++;
            DEBUG_WARN(("ERROR item's desc record is NULL\n"));
            continue;
        }
        DEBUG_INFO(("Desc Email ID %#" PRIx64 " [d_ptr->d_id = %#" PRIx64 "]\n", d_ptr->desc->i_id, d_ptr->d_id));

        item = pst_parse_item(&pstfile, d_ptr, NULL);
        DEBUG_INFO(("About to process item\n"));

        if (!item) {
            ff.skip_count++;
            DEBUG_INFO(("A NULL item was seen\n"));
            continue;
        }

        if (item->subject.str) {
            DEBUG_INFO(("item->subject = %s\n", item->subject.str));
        }

        if (item->folder && item->file_as.str) {
            DEBUG_INFO(("Processing Folder \"%s\"\n", item->file_as.str));
            if (output_mode != OUTPUT_QUIET) {
                pst_debug_lock();
                    printf("Processing Folder \"%s\"\n", item->file_as.str);
                    fflush(stdout);
                pst_debug_unlock();
            }
            ff.item_count++;
            if (d_ptr->child && (deleted_mode == DMODE_INCLUDE || strcasecmp(item->file_as.str, "Deleted Items"))) {
                //if this is a non-empty folder other than deleted items, we want to recurse into it
                pid_t parent = getpid();
                pid_t child = try_fork(item->file_as.str);
                if (child == 0) {
                    // we are the child process, or the original parent if no children were available
                    pid_t me = getpid();
                    process(item, d_ptr->child);
#ifdef HAVE_FORK
#ifdef HAVE_SEMAPHORE_H
                    if (me != parent) {
                        // we really were a child, forked for the sole purpose of processing this folder
                        // free my child count slot before really exiting, since
                        // all I am doing here is waiting for my children to exit
                        sem_post(global_children);
                        grim_reaper(1); // wait for all my child processes to exit
                        exit(0);        // really exit
                    }
#endif
#endif
                }
            }

        } else if (item->contact && (item->type == PST_TYPE_CONTACT)) {
            DEBUG_INFO(("Processing Contact\n"));
            if (!(output_type_mode & OTMODE_CONTACT)) {
                ff.skip_count++;
                DEBUG_INFO(("skipping contact: not in output type list\n"));
            }
            else {
                ff.item_count++;
                if (mode == MODE_SEPARATE) mk_separate_file(&ff, PST_TYPE_CONTACT, (mode_EX) ? ".vcf" : "", 1);
                if (contact_mode == CMODE_VCARD) {
                    pst_convert_utf8_null(item, &item->comment);
                    write_vcard(ff.output[PST_TYPE_CONTACT], item, item->contact, item->comment.str);
                }
                else {
                    pst_convert_utf8(item, &item->contact->fullname);
                    pst_convert_utf8(item, &item->contact->address1);
                    fprintf(ff.output[PST_TYPE_CONTACT], "%s <%s>\n", item->contact->fullname.str, item->contact->address1.str);
                }
                if (mode == MODE_SEPARATE) close_separate_file(&ff);
            }

        } else if (item->email && ((item->type == PST_TYPE_NOTE) || (item->type == PST_TYPE_SCHEDULE) || (item->type == PST_TYPE_REPORT))) {
            DEBUG_INFO(("Processing Email\n"));
            if (!(output_type_mode & OTMODE_EMAIL)) {
                ff.skip_count++;
                DEBUG_INFO(("skipping email: not in output type list\n"));
            }
            else {
                char *extra_mime_headers = NULL;
                ff.item_count++;
                if (mode == MODE_SEPARATE) {
                    // process this single email message, possibly forking
                    pid_t parent = getpid();
                    pid_t child = try_fork(item->file_as.str);
                    if (child == 0) {
                        // we are the child process, or the original parent if no children were available
                        pid_t me = getpid();
                        mk_separate_file(&ff, PST_TYPE_NOTE, (mode_EX) ? ".eml" : "", 1);
                        write_normal_email(ff.output[PST_TYPE_NOTE], ff.name[PST_TYPE_NOTE], item, mode, mode_MH, &pstfile, save_rtf_body, PST_TYPE_NOTE, &extra_mime_headers);
                        close_separate_file(&ff);
                        if (mode_MSG) {
                            mk_separate_file(&ff, PST_TYPE_NOTE, ".msg", 0);
                            write_msg_email(ff.name[PST_TYPE_NOTE], item, &pstfile);
                        }
#ifdef HAVE_FORK
#ifdef HAVE_SEMAPHORE_H
                        if (me != parent) {
                            // we really were a child, forked for the sole purpose of processing this message
                            // free my child count slot before really exiting, since
                            // all I am doing here is waiting for my children to exit
                            sem_post(global_children);
                            grim_reaper(1); // wait for all my child processes to exit - there should not be any
                            exit(0);        // really exit
                        }
#endif
#endif
                    }
                }
                else {
                    // process this single email message, cannot fork since not separate mode
                    write_normal_email(ff.output[PST_TYPE_NOTE], ff.name[PST_TYPE_NOTE], item, mode, mode_MH, &pstfile, save_rtf_body, 0, &extra_mime_headers);
                }
            }

        } else if (item->journal && (item->type == PST_TYPE_JOURNAL)) {
            DEBUG_INFO(("Processing Journal Entry\n"));
            if (!(output_type_mode & OTMODE_JOURNAL)) {
                ff.skip_count++;
                DEBUG_INFO(("skipping journal entry: not in output type list\n"));
            }
            else {
                ff.item_count++;
                if (mode == MODE_SEPARATE) mk_separate_file(&ff, PST_TYPE_JOURNAL, (mode_EX) ? ".ics" : "", 1);
                write_journal(ff.output[PST_TYPE_JOURNAL], item);
                fprintf(ff.output[PST_TYPE_JOURNAL], "\n");
                if (mode == MODE_SEPARATE) close_separate_file(&ff);
            }

        } else if (item->appointment && (item->type == PST_TYPE_APPOINTMENT)) {
            DEBUG_INFO(("Processing Appointment Entry\n"));
            if (!(output_type_mode & OTMODE_APPOINTMENT)) {
                ff.skip_count++;
                DEBUG_INFO(("skipping appointment: not in output type list\n"));
            }
            else {
                ff.item_count++;
                if (mode == MODE_SEPARATE) mk_separate_file(&ff, PST_TYPE_APPOINTMENT, (mode_EX) ? ".ics" : "", 1);
                write_schedule_part_data(ff.output[PST_TYPE_APPOINTMENT], item, NULL, NULL);
                fprintf(ff.output[PST_TYPE_APPOINTMENT], "\n");
                if (mode == MODE_SEPARATE) close_separate_file(&ff);
            }

        } else if (item->message_store) {
            // there should only be one message_store, and we have already done it
            ff.skip_count++;
            DEBUG_WARN(("item with message store content, type %i %s, skipping it\n", item->type, item->ascii_type));

        } else {
            ff.skip_count++;
            DEBUG_WARN(("Unknown item type %i (%s) name (%s)\n",
                        item->type, item->ascii_type, item->file_as.str));
        }
        pst_freeItem(item);
    }
    close_enter_dir(&ff);
    DEBUG_RET();
}



int main(int argc, char* const* argv) {
    pst_item *item = NULL;
    pst_desc_tree *d_ptr;
    char * fname = NULL;
    char *d_log  = NULL;
    int c,x;
    char *temp = NULL;               //temporary char pointer
    prog_name = argv[0];

    if (regcomp(&meta_charset_pattern, "<meta[^>]*content=\"[^>]*charset=([^>\";]*)[\";]", REG_ICASE | REG_EXTENDED)) {
        printf("cannot compile regex pattern to find content charset in html bodies\n");
        exit(3);
    }

    // command-line option handling
    while ((c = getopt(argc, argv, "a:bC:c:Dd:emhj:kMo:qrSt:uVwL:8"))!= -1) {
        switch (c) {
        case 'a':
            if (optarg) {
                int n = strlen(optarg);
                acceptable_extensions = (char*)pst_malloc(n+2);
                strcpy(acceptable_extensions, optarg);
                acceptable_extensions[n+1] = '\0';  // double null terminates array of non-empty null terminated strings.
                char *p = acceptable_extensions;
                while (*p) {
                    if (*p == ',') *p = '\0';
                    p++;
                }
            }
            break;
        case 'b':
            save_rtf_body = 0;
            break;
        case 'C':
            if (optarg) {
                default_charset = optarg;
            }
            else {
                usage();
                exit(0);
            }
            break;
        case 'c':
            if (optarg && optarg[0]=='v') {
                contact_mode=CMODE_VCARD;
                contact_mode_specified = 1;
            }
            else if (optarg && optarg[0]=='l') {
                contact_mode=CMODE_LIST;
                contact_mode_specified = 1;
            }
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
        case 'j':
            max_children = atoi(optarg);
            max_child_specified = 1;
            break;
        case 'k':
            mode = MODE_KMAIL;
            break;
        case 'M':
            mode = MODE_SEPARATE;
            mode_MH  = 1;
            mode_EX  = 0;
            mode_MSG = 0;
            break;
        case 'e':
            mode = MODE_SEPARATE;
            mode_MH  = 1;
            mode_EX  = 1;
            mode_MSG = 0;
            file_name_len = 14;
            break;
        case 'L':
            pst_debug_setlevel(atoi(optarg));
            break;
        case 'm':
            mode = MODE_SEPARATE;
            mode_MH  = 1;
            mode_EX  = 1;
            mode_MSG = 1;
            file_name_len = 14;
            break;
        case 'o':
            output_dir = optarg;
            break;
        case 'q':
            output_mode = OUTPUT_QUIET;
            break;
        case 'r':
            mode = MODE_RECURSE;
            mode_thunder = 0;
            break;
        case 'S':
            mode = MODE_SEPARATE;
            mode_MH  = 0;
            mode_EX  = 0;
            mode_MSG = 0;
            break;
        case 't':
            // email, appointment, contact, other
            if (!optarg) {
                usage();
                exit(0);
            }
            temp = optarg;
            output_type_mode = 0;
            while (*temp > 0) {
              switch (temp[0]) {
                case 'e':
                    output_type_mode |= OTMODE_EMAIL;
                    break;
                case 'a':
                    output_type_mode |= OTMODE_APPOINTMENT;
                    break;
                case 'j':
                    output_type_mode |= OTMODE_JOURNAL;
                    break;
                case 'c':
                    output_type_mode |= OTMODE_CONTACT;
                    break;
                default:
                    usage();
                    exit(0);
                    break;
              }
              temp++;
            }
            break;
        case 'u':
            mode = MODE_RECURSE;
            mode_thunder = 1;
            break;
        case 'V':
            version();
            exit(0);
            break;
        case 'w':
            overwrite = 1;
            break;
        case '8':
            prefer_utf8 = 1;
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

#ifdef _SC_NPROCESSORS_ONLN
    number_processors =  sysconf(_SC_NPROCESSORS_ONLN);
#endif
    max_children    = (max_child_specified) ? max_children : number_processors * 4;
    active_children = 0;
    child_processes = (pid_t *)pst_malloc(sizeof(pid_t) * max_children);
    memset(child_processes, 0, sizeof(pid_t) * max_children);

#if defined(HAVE_SEMAPHORE_H) && defined(HAVE_SYS_IPC_H) && defined(HAVE_SYS_SHM_H)
    if (max_children) {
        shared_memory_id = shmget(IPC_PRIVATE, sizeof(sem_t)*2, 0777);
        if (shared_memory_id >= 0) {
            global_children = (sem_t *)shmat(shared_memory_id, NULL, 0);
            if (global_children == (sem_t *)-1) global_children = NULL;
            if (global_children) {
                output_mutex = &(global_children[1]);
                sem_init(global_children, 1, max_children);
                sem_init(output_mutex, 1, 1);
            }
            shmctl(shared_memory_id, IPC_RMID, NULL);
        }
    }
#endif

    #ifdef DEBUG_ALL
        // force a log file
        if (!d_log) d_log = "readpst.log";
    #endif // defined DEBUG_ALL
    #ifdef HAVE_SEMAPHORE_H
        DEBUG_INIT(d_log, output_mutex);
    #else
        DEBUG_INIT(d_log, NULL);
    #endif
    DEBUG_ENT("main");

    if (output_mode != OUTPUT_QUIET) printf("Opening PST file and indexes...\n");
    RET_DERROR(pst_open(&pstfile, fname, default_charset), 1, ("Error opening File\n"));
    RET_DERROR(pst_load_index(&pstfile), 2, ("Index Error\n"));

    pst_load_extended_attributes(&pstfile);

    if (chdir(output_dir)) {
        x = errno;
        pst_close(&pstfile);
        DEBUG_RET();
        DIE(("Cannot change to output dir %s: %s\n", output_dir, strerror(x)));
    }

    d_ptr = pstfile.d_head; // first record is main record
    item  = pst_parse_item(&pstfile, d_ptr, NULL);
    if (!item || !item->message_store) {
        DEBUG_RET();
        DIE(("Could not get root record\n"));
    }

    // default the file_as to the same as the main filename if it doesn't exist
    if (!item->file_as.str) {
        if (!(temp = strrchr(fname, '/')))
            if (!(temp = strrchr(fname, '\\')))
                temp = fname;
            else
                temp++; // get past the "\\"
        else
            temp++; // get past the "/"
        item->file_as.str = (char*)pst_malloc(strlen(temp)+1);
        strcpy(item->file_as.str, temp);
        item->file_as.is_utf8 = 1;
        DEBUG_INFO(("file_as was blank, so am using %s\n", item->file_as.str));
    }
    DEBUG_INFO(("Root Folder Name: %s\n", item->file_as.str));

    d_ptr = pst_getTopOfFolders(&pstfile, item);
    if (!d_ptr) {
        DEBUG_RET();
        DIE(("Top of folders record not found. Cannot continue\n"));
    }

    process(item, d_ptr->child);    // do the children of TOPF
    grim_reaper(1); // wait for all child processes

    pst_freeItem(item);
    pst_close(&pstfile);
    DEBUG_RET();
    DEBUG_CLOSE();

#ifdef HAVE_SEMAPHORE_H
    if (global_children) {
        sem_destroy(global_children);
        sem_destroy(output_mutex);
        shmdt(global_children);
    }
#endif
    free(child_processes);

    regfree(&meta_charset_pattern);
    return 0;
}


void write_email_body(FILE *f, char *body) {
    char *n = body;
    DEBUG_ENT("write_email_body");
    if (mode != MODE_SEPARATE) {
        while (n) {
            char *p = body;
            while (*p == '>') p++;
            if (strncmp(p, "From ", 5) == 0) fprintf(f, ">");
            if ((n = strchr(body, '\n'))) {
                n++;
                pst_fwrite(body, n-body, 1, f); //write just a line
                body = n;
            }
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
    printf("\t-C charset\t- character set for items with an unspecified character set\n");
    printf("\t-D\t- Include deleted items in output\n");
    printf("\t-L <level> \t- Set debug level; 1=debug,2=info,3=warn.\n");
    printf("\t-M\t- Write emails in the MH (rfc822) format\n");
    printf("\t-S\t- Separate. Write emails in the separate format\n");
    printf("\t-a <attachment-extension-list>\t- Discard any attachment without an extension on the list\n");
    printf("\t-b\t- Don't save RTF-Body attachments\n");
    printf("\t-c[v|l]\t- Set the Contact output mode. -cv = VCard, -cl = EMail list\n");
    printf("\t-d <filename> \t- Debug to file.\n");
    printf("\t-e\t- As with -M, but include extensions on output files\n");
    printf("\t-h\t- Help. This screen\n");
    printf("\t-j <integer>\t- Number of parallel jobs to run\n");
    printf("\t-k\t- KMail. Output in kmail format\n");
    printf("\t-m\t- As with -e, but write .msg files also\n");
    printf("\t-o <dirname>\t- Output directory to write files to. CWD is changed *after* opening pst file\n");
    printf("\t-q\t- Quiet. Only print error messages\n");
    printf("\t-r\t- Recursive. Output in a recursive format\n");
    printf("\t-t[eajc]\t- Set the output type list. e = email, a = attachment, j = journal, c = contact\n");
    printf("\t-u\t- Thunderbird mode. Write two extra .size and .type files\n");
    printf("\t-w\t- Overwrite any output mbox files\n");
    printf("\t-8\t- Output bodies in UTF-8, rather than original encoding, if UTF-8 version is available\n");
    printf("\n");
    printf("Only one of -M -S -e -k -m -r should be specified\n");
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
    DEBUG_RET();
}


void mk_kmail_dir(char *fname) {
    //make a directory based on OUTPUT_KMAIL_DIR_TEMPLATE
    //change to that directory
    char *dir, *index;
    int x;
    DEBUG_ENT("mk_kmail_dir");
    dir = pst_malloc(strlen(fname)+strlen(OUTPUT_KMAIL_DIR_TEMPLATE)+1);
    sprintf(dir, OUTPUT_KMAIL_DIR_TEMPLATE, fname);
    check_filename(dir);
    if (D_MKDIR(dir)) {
        if (errno != EEXIST) {  // not an error because it exists
            x = errno;
            DIE(("mk_kmail_dir: Cannot create directory %s: %s\n", dir, strerror(x)));
        }
    }
    if (chdir(dir)) {
        x = errno;
        DIE(("mk_kmail_dir: Cannot change to directory %s: %s\n", dir, strerror(x)));
    }
    free (dir);

    //we should remove any existing indexes created by KMail, cause they might be different now
    index = pst_malloc(strlen(fname)+strlen(KMAIL_INDEX)+1);
    sprintf(index, KMAIL_INDEX, fname);
    unlink(index);
    free(index);

    DEBUG_RET();
}


int close_kmail_dir() {
    int x;
    DEBUG_ENT("close_kmail_dir");
    if (chdir("..")) {
        x = errno;
        DIE(("close_kmail_dir: Cannot move up dir (..): %s\n", strerror(x)));
    }
    DEBUG_RET();
    return 0;
}


char *item_type_to_name(int32_t item_type) {
    char *name;
    switch (item_type) {
        case PST_TYPE_APPOINTMENT:
            name = "calendar";
            break;
        case PST_TYPE_CONTACT:
            name = "contacts";
            break;
        case PST_TYPE_JOURNAL:
            name = "journal";
            break;
        case PST_TYPE_STICKYNOTE:
        case PST_TYPE_TASK:
        case PST_TYPE_NOTE:
        case PST_TYPE_OTHER:
        case PST_TYPE_REPORT:
        default:
            name = "mbox";
            break;
    }
    return name;
}


int32_t reduced_item_type(int32_t item_type) {
    int32_t reduced;
    switch (item_type) {
        case PST_TYPE_APPOINTMENT:
        case PST_TYPE_CONTACT:
        case PST_TYPE_JOURNAL:
            reduced = item_type;
            break;
        case PST_TYPE_STICKYNOTE:
        case PST_TYPE_TASK:
        case PST_TYPE_NOTE:
        case PST_TYPE_OTHER:
        case PST_TYPE_REPORT:
        default:
            reduced = PST_TYPE_NOTE;
            break;
    }
    return reduced;
}


// this will create a directory by that name
void mk_recurse_dir(char *dir) {
    int x;
    DEBUG_ENT("mk_recurse_dir");
    check_filename(dir);
    if (D_MKDIR (dir)) {
        if (errno != EEXIST) {  // not an error because it exists
            x = errno;
            DIE(("mk_recurse_dir: Cannot create directory %s: %s\n", dir, strerror(x)));
        }
    }
    if (chdir(dir)) {
        x = errno;
        DIE(("mk_recurse_dir: Cannot change to directory %s: %s\n", dir, strerror(x)));
    }
    DEBUG_RET();
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


void mk_separate_dir(char *dir) {
    size_t dirsize = strlen(dir) + 10;
    char dir_name[dirsize];
    int x = 0, y = 0;

    DEBUG_ENT("mk_separate_dir");
    do {
        if (y == 0)
            snprintf(dir_name, dirsize, "%s", dir);
        else
            snprintf(dir_name, dirsize, "%s%i%s", dir, y, ""); // enough for 9 digits allocated above

        check_filename(dir_name);
        DEBUG_INFO(("about to try creating %s\n", dir_name));
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
            DEBUG_WARN(("mk_separate_dir: Cannot open dir \"%s\" for deletion of old contents\n", "./"));
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
            closedir(sdir);     // cppcheck detected leak
        }
#endif
    }

    DEBUG_RET();
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


void mk_separate_file(struct file_ll *f, int32_t t, char *extension, int openit) {
    DEBUG_ENT("mk_separate_file");
    DEBUG_INFO(("opening next file to save email type %s\n", item_type_to_name(t)));
    if (f->item_count > 999999999) { // bigger than nine 9's
        DIE(("mk_separate_file: The number of emails in this folder has become too high to handle\n"));
    }
    sprintf(f->name[t], "%" PRIi32 "%s", f->item_count, extension);
    check_filename(f->name[t]);
    if (openit) {
        if (!(f->output[t] = fopen(f->name[t], "w"))) {
            DIE(("mk_separate_file: Cannot open file to save email \"%s\"\n", f->name[t]));
        }
    }
    DEBUG_RET();
}


void close_separate_file(struct file_ll *f) {
    int32_t t;
    DEBUG_ENT("close_separate_file");
    for (t=0; t<PST_TYPE_MAX; t++) {
        if (f->output[t]) {
            struct stat st;
            fclose(f->output[t]);
            stat(f->name[t], &st);
            if (!st.st_size) {
                DEBUG_WARN(("removing empty output file %s\n", f->name[t]));
                remove(f->name[t]);
            }
            f->output[t] = NULL;
        }
    }
    DEBUG_RET();
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


/**
 * check if the file name extension is acceptable. If not, the attachment
 * will be discarded
 * @param attach  pst attachment object
 * @return        true if the attachment filename contains an extension that we want.
 */
int  acceptable_ext(pst_item_attach* attach)
{
    if (!acceptable_extensions || *acceptable_extensions == '\0') return 1;     // acceptable list missing or empty
    char *attach_filename = (attach->filename2.str) ? attach->filename2.str
                                                    : attach->filename1.str;
    if (!attach_filename) return 1; // attachment with no name is always acceptable
    char *e = strrchr(attach_filename, '.');
    if (!e) return 1;               // attachment with no extension is always acceptable.
    DEBUG_ENT("acceptable_ext");
    DEBUG_INFO(("attachment extension %s\n", e));
    int rc = 0;
    char *a = acceptable_extensions;
    while (*a) {
        if (pst_stricmp(a, e) == 0) {
            rc = 1;
            break;
        }
        a += strlen(a) + 1;
    }
    DEBUG_INFO(("attachment acceptable returns %d\n", rc));
    DEBUG_RET();
    return rc;
}


void write_separate_attachment(char f_name[], pst_item_attach* attach, int attach_num, pst_file* pst)
{
    FILE *fp = NULL;
    int x = 0;
    char *temp = NULL;

    // If there is a long filename (filename2) use that, otherwise
    // use the 8.3 filename (filename1)
    char *attach_filename = (attach->filename2.str) ? attach->filename2.str
                                                    : attach->filename1.str;
    DEBUG_ENT("write_separate_attachment");
    DEBUG_INFO(("Attachment %s Size is %#zx, data = %p, id %#" PRIx64 "\n", attach_filename, attach->data.size, (void*)attach->data.data, attach->i_id));

    if (!attach->data.data) {
        // make sure we can fetch data from the id
        pst_index_ll *ptr = pst_getID(pst, attach->i_id);
        if (!ptr) {
            DEBUG_WARN(("Couldn't find i_id %#" PRIx64 ". Cannot save attachment to file\n", attach->i_id));
            DEBUG_RET();
            return;
        }
    }

    check_filename(f_name);
    if (!attach_filename) {
        // generate our own (dummy) filename for the attachment
        temp = pst_malloc(strlen(f_name)+15);
        sprintf(temp, "%s-attach%i", f_name, attach_num);
    } else {
        // have an attachment name, make sure it's unique
        temp = pst_malloc(strlen(f_name)+strlen(attach_filename)+15);
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
    DEBUG_INFO(("Saving attachment to %s\n", temp));
    if (!(fp = fopen(temp, "w"))) {
        DEBUG_WARN(("write_separate_attachment: Cannot open attachment save file \"%s\"\n", temp));
    } else {
        (void)pst_attach_to_file(pst, attach, fp);
        fclose(fp);
    }
    if (temp) free(temp);
    DEBUG_RET();
}


void write_embedded_message(FILE* f_output, pst_item_attach* attach, char *boundary, pst_file* pf, int save_rtf, char** extra_mime_headers)
{
    pst_index_ll *ptr;
    DEBUG_ENT("write_embedded_message");
    ptr = pst_getID(pf, attach->i_id);

    pst_desc_tree d_ptr;
    d_ptr.d_id        = 0;
    d_ptr.parent_d_id = 0;
    d_ptr.assoc_tree  = NULL;
    d_ptr.desc        = ptr;
    d_ptr.no_child    = 0;
    d_ptr.prev        = NULL;
    d_ptr.next        = NULL;
    d_ptr.parent      = NULL;
    d_ptr.child       = NULL;
    d_ptr.child_tail  = NULL;

    pst_item *item = pst_parse_item(pf, &d_ptr, attach->id2_head);
    // It appears that if the embedded message contains an appointment/
    // calendar item, pst_parse_item returns NULL due to the presence of
    // an unexpected reference type of 0x1048, which seems to represent
    // an array of GUIDs representing a CLSID. It's likely that this is
    // a reference to an internal Outlook COM class.
    //      Log the skipped item and continue on.
    if (!item) {
        DEBUG_WARN(("write_embedded_message: pst_parse_item was unable to parse the embedded message in attachment ID %" PRIu64, attach->i_id));
    } else {
        if (!item->email) {
            DEBUG_WARN(("write_embedded_message: pst_parse_item returned type %d, not an email message", item->type));
        } else {
            fprintf(f_output, "\n--%s\n", boundary);
            fprintf(f_output, "Content-Type: %s\n\n", attach->mimetype.str);
            write_normal_email(f_output, "", item, MODE_NORMAL, 0, pf, save_rtf, 1, extra_mime_headers);
        }
        pst_freeItem(item);
    }

    DEBUG_RET();
}

/**
 * Backslash-escape quotes and backslashes in the given string.
 */
char *quote_string(char *inp) {
    int i = 0;
    int count = 0;
    char *curr = inp;
    while (*curr) {
        if (*curr == '\"' || *curr == '\\') {
            count++;
        }
        curr++;
        i++;
    }
    char *res = malloc(i + count + 1);
    char *curr_in = inp;
    char *curr_out = res;
    while (*curr_in) {
        if (*curr_in == '\"' || *curr_in == '\\') {
            *curr_out++ = '\\';
        }
        *curr_out++ = *curr_in++;
    }
    *curr_out = '\0';
    return res;
}

/*
 * RFC 2231 says:
 *   attribute-char := <any (US-ASCII) CHAR except SPACE, CTLs,
 *                   "*", "'", "%", or tspecials>
 * Equivalent, but clearer from RFC 5987:
 *   attr-char     = ALPHA / DIGIT
 *                   / "!" / "#" / "$" / "&" / "+" / "-" / "."
 *                   / "^" / "_" / "`" / "|" / "~"
 */
static int is_attribute_char(int8_t x) {
    if ((x >= 'A' && x <= 'Z') ||
        (x >= 'a' && x <= 'z') ||
        (x >= '0' && x <= '9')) {
           return 1;
    }
    switch(x) {
        case '!':
        case '#':
        case '$':
        case '&':
        case '+':
        case '-':
        case '.':
        case '^':
        case '_':
        case '`':
        case '|':
        case '~':
            return 1;
    }
    return 0;
}

/** Convert inp to rfc2231 encoding of string
 *
 *  @param inp   pointer to the string of interest
 *  @return      pointer to converted string -- caller must free
 */
char *rfc2231_string(char *inp) {
    int needs = 0;
    const int8_t *x = (int8_t *)inp;
    while (*x) {
        if (!is_attribute_char(*x)) needs++;
        x++;
    }
    int n = strlen(inp) + 2*needs + 15;
    char *buffer = pst_malloc(n);
    strcpy(buffer, "utf-8''");
    x = (int8_t *)inp;
    const uint8_t *y = (uint8_t *)inp;
    char *z = buffer;
    z += strlen(buffer);    // skip the utf8 prefix
    while (*y) {
        if (!is_attribute_char(*x)) {
            *(z++) = '%';
            snprintf(z, 3, "%2" PRIx8, *y);
            z += 2;
        }
        else {
            *(z++) = *y;
        }
        x++;
        y++;
    }
    *z = '\0';
    return buffer;
}

void write_inline_attachment(FILE* f_output, pst_item_attach* attach, char *boundary, pst_file* pst)
{
    DEBUG_ENT("write_inline_attachment");
    DEBUG_INFO(("Attachment Size is %#zx, data = %p, id %#" PRIx64 "\n", attach->data.size, (void*)attach->data.data, attach->i_id));

    if (!attach->data.data) {
        // make sure we can fetch data from the id
        pst_index_ll *ptr = pst_getID(pst, attach->i_id);
        if (!ptr) {
            DEBUG_WARN(("Couldn't find ID pointer. Cannot save attachment to file\n"));
            DEBUG_RET();
            return;
        }
    }

    fprintf(f_output, "\n--%s\n", boundary);
    if (!attach->mimetype.str) {
        fprintf(f_output, "Content-Type: %s\n", MIME_TYPE_DEFAULT);
    } else {
        fprintf(f_output, "Content-Type: %s\n", attach->mimetype.str);
    }
    fprintf(f_output, "Content-Transfer-Encoding: base64\n");

    if (attach->content_id.str) {
        fprintf(f_output, "Content-ID: <%s>\n", attach->content_id.str);
    }

    if (attach->filename2.str) {
        // use the long filename, converted to proper encoding if needed.
        // it is already utf8
        char *escaped = quote_string(attach->filename2.str);
        // encode long filename as rfc2231 without modifying original -- we may still need the original long filename
        char *rfc2231 = rfc2231_string(attach->filename2.str);
        fprintf(f_output, "Content-Disposition: attachment; \n        filename*=%s;\n", rfc2231);
        free (rfc2231);
        // Also include the (escaped) utf8 filename in the 'filename' header directly - this is not strictly valid
        // (since this header should be ASCII) but is almost always handled correctly (and in fact this is the only
        // way to get MS Outlook to correctly read a UTF8 filename, AFAICT, which is why we're doing it).
        fprintf(f_output, "        filename=\"%s\"\n\n", escaped);
        free(escaped);
    }
    else if (attach->filename1.str) {
        // short filename never needs encoding
        fprintf(f_output, "Content-Disposition: attachment; filename=\"%s\"\n\n", attach->filename1.str);
    }
    else {
        // no filename is inline
        fprintf(f_output, "Content-Disposition: inline\n\n");
    }

    (void)pst_attach_to_file_base64(pst, attach, f_output);
    fprintf(f_output, "\n\n");
    DEBUG_RET();
}


int  header_match(char *header, char*field) {
    int n = strlen(field);
    if (strncasecmp(header, field, n) == 0) return 1;   // tag:{space}
    if ((field[n-1] == ' ') && (strncasecmp(header, field, n-1) == 0)) {
        char *crlftab = "\r\n\t";
        char *crlfspc = "\r\n ";
        DEBUG_INFO(("Possible wrapped header = %s\n", header));
        if (strncasecmp(header+n-1, crlftab, 3) == 0) return 1; // tag:{cr}{lf}{tab}
        if (strncasecmp(header+n-1, crlfspc, 3) == 0) return 1; // tag:{cr}{lf}{space}
    }
    return 0;
}

// https://en.wikipedia.org/wiki/Email#Message_header
// https://www.rfc-editor.org/rfc/rfc5322.html
// https://www.iana.org/assignments/message-headers/message-headers.xhtml
int  header_is_reasonable(char *header)
{
    char *c;
#define C *c

    // The header must not be NULL
    if (header) c = header;
    else return 0;

    // usually the header field name starts with upper-case: A-Z
    if (C >= 'A' && C <= 'Z') c++;
    else return 0;

    while(1) {
        // most header field names use a limited set of characters: - 0-9 A-Z a-z
        if (
            (C >= 'A' && C <= 'Z') ||
            (C >= 'a' && C <= 'z') ||
            (C >= '0' && C <= '9') ||
            (C == '-')
           ) {
            c++;
        // the header field name is then terminated with a colon
        } else if (C == ':') {
          c++;
          goto parse_header_field_value;
        // other characters are an indicator of an invalid header
        } else {
          return 0;
        }
    }

parse_header_field_value:
    while(1) {
        // header field values are printable US-ASCII plus space/tab
        if (
            (C >= 33 && C <= 126) ||
            (C == ' ' || C == '\t')
           ) {
            c++;
        // the header field value is then terminated with CRLF
        } else if (C == '\r' && *(c+1) == '\n') {
            c += 2;
            // the value could continue to the next line though
            if (C == ' ' || C == '\t') c++;
            else return 1;
        // other characters are an indicator of an invalid header
        } else {
          return 0;
        }
    }

#undef C

}

int  valid_headers(char *header)
{
    // headers are sometimes really bogus - they seem to be fragments of the
    // message body, so we only use them if they seem to be real rfc822 headers.
    // this list is composed of ones that we have seen in real pst files.
    // there are surely others. the problem is - given an arbitrary character
    // string, is it a valid (or even reasonable) set of rfc822 headers?
    if (header) {
        if (header_match(header, "Content-Language: "             )) return 1;
        if (header_match(header, "Content-Type: "                 )) return 1;
        if (header_match(header, "DKIM-Signature: "               )) return 1;
        if (header_match(header, "Date: "                         )) return 1;
        if (header_match(header, "Delivered-To: "                 )) return 1;
        if (header_match(header, "From: "                         )) return 1;
        if (header_match(header, "MIME-Version: "                 )) return 1;
        if (header_match(header, "Microsoft Mail Internet Headers")) return 1;
        if (header_match(header, "Received: "                     )) return 1;
        if (header_match(header, "Return-Path: "                  )) return 1;
        if (header_match(header, "Subject: "                      )) return 1;
        if (header_match(header, "To: "                           )) return 1;
        if (header_match(header, "User-Agent: "                   )) return 1;
        if (header_match(header, "X-ASG-Debug-ID: "               )) return 1;
        if (header_match(header, "X-Barracuda-URL: "              )) return 1;
        if (header_match(header, "X-x: "                          )) return 1;
        if (header_is_reasonable(header)) return 1;
        if (strlen(header) > 2) {
            DEBUG_INFO(("Ignore bogus headers = %s\n", header));
        }
        return 0;
    }
    else return 0;
}


void header_has_field(char *header, char *field, int *flag)
{
    DEBUG_ENT("header_has_field");
    if (my_stristr(header, field) || (strncasecmp(header, field+1, strlen(field)-1) == 0)) {
        DEBUG_INFO(("header block has %s header\n", field+1));
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
        DEBUG_INFO(("body %s %s from headers\n", subfield, body_subfield));
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
    char *t;
    while ((t = header_get_field(header, field))) {
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


int  test_base64(char *body, size_t len)
{
    int b64 = 0;
    uint8_t *b = (uint8_t *)body;
    DEBUG_ENT("test_base64");
    while (len--) {
        if ((*b < 32) && (*b != 9) && (*b != 10)) {
            DEBUG_INFO(("found base64 byte %" PRId8 "\n", *b));
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
            DEBUG_INFO(("charset %s from html text\n", charset));
        }
        else {
            DEBUG_INFO(("matching %d %d %d %d\n", match[0].rm_so, match[0].rm_eo, match[1].rm_so, match[1].rm_eo));
            DEBUG_HEXDUMPC(html, strlen(html), 0x10);
        }
    }
    else {
        DEBUG_INFO(("regexec returns %d\n", rc));
    }
    DEBUG_RET();
}


void find_rfc822_headers(char** extra_mime_headers)
{
    DEBUG_ENT("find_rfc822_headers");
    char *headers = *extra_mime_headers;
    if (headers) {
        char *temp, *t;
        while ((temp = strstr(headers, "\n\n"))) {
            temp[1] = '\0';
            t = header_get_field(headers, "\nContent-Type:");
            if (t) {
                t++;
                DEBUG_INFO(("found content type header\n"));
                char *n = strchr(t, '\n');
                char *s = strstr(t, ": ");
                char *e = strchr(t, ';');
                if (!e || (e > n)) e = n;
                if (s && (s < e)) {
                    s += 2;
                    if (!strncasecmp(s, RFC822, e-s)) {
                        headers = temp+2;   // found rfc822 header
                        DEBUG_INFO(("found 822 headers\n%s\n", headers));
                        break;
                    }
                }
            }
            //DEBUG_INFO(("skipping to next block after\n%s\n", headers));
            headers = temp+2;   // skip to next chunk of headers
        }
        *extra_mime_headers = headers;
    }
    DEBUG_RET();
}


void write_body_part(FILE* f_output, pst_string *body, char *mime, char *charset, char *boundary, pst_file* pst)
{
    DEBUG_ENT("write_body_part");
    removeCR(body->str);
    size_t body_len = strlen(body->str);

    if (body->is_utf8 && (strcasecmp("utf-8", charset))) {
        if (prefer_utf8) {
            charset = "utf-8";
        } else {
            // try to convert to the specified charset since the target
            // is not utf-8, and the data came from a unicode (utf16) field
            // and is now in utf-8.
            size_t rc;
            DEBUG_INFO(("Convert %s utf-8 to %s\n", mime, charset));
            pst_vbuf *newer = pst_vballoc(2);
            rc = pst_vb_utf8to8bit(newer, body->str, body_len, charset);
            if (rc == (size_t)-1) {
                // unable to convert, change the charset to utf8
                free(newer->b);
                DEBUG_INFO(("Failed to convert %s utf-8 to %s\n", mime, charset));
                charset = "utf-8";
            } else {
                // null terminate the output string
                pst_vbgrow(newer, 1);
                newer->b[newer->dlen] = '\0';
                free(body->str);
                body->str = newer->b;
                body_len = newer->dlen;
            }
            free(newer);
        }
    }
    int base64 = test_base64(body->str, body_len);
    fprintf(f_output, "\n--%s\n", boundary);
    fprintf(f_output, "Content-Type: %s; charset=\"%s\"\n", mime, charset);
    if (base64) fprintf(f_output, "Content-Transfer-Encoding: base64\n");
    fprintf(f_output, "\n");
    // Any body that uses an encoding with NULLs, e.g. UTF16, will be base64-encoded here.
    if (base64) {
        char *enc = pst_base64_encode(body->str, body_len);
        if (enc) {
            write_email_body(f_output, enc);
            fprintf(f_output, "\n");
            free(enc);
        }
    }
    else {
        write_email_body(f_output, body->str);
    }
    DEBUG_RET();
}


void write_schedule_part_data(FILE* f_output, pst_item* item, const char* sender, const char* method)
{
    fprintf(f_output, "BEGIN:VCALENDAR\n");
    fprintf(f_output, "VERSION:2.0\n");
    fprintf(f_output, "PRODID:LibPST v%s\n", VERSION);
    if (method) fprintf(f_output, "METHOD:%s\n", method);
    fprintf(f_output, "BEGIN:VEVENT\n");
    if (sender) {
        if (item->email->outlook_sender_name.str) {
            fprintf(f_output, "ORGANIZER;CN=\"%s\":MAILTO:%s\n", item->email->outlook_sender_name.str, sender);
        } else {
            fprintf(f_output, "ORGANIZER;CN=\"\":MAILTO:%s\n", sender);
        }
    }
    write_appointment(f_output, item);
    fprintf(f_output, "END:VCALENDAR\n");
}


void write_schedule_part(FILE* f_output, pst_item* item, const char* sender, const char* boundary)
{
    const char* method  = "REQUEST";
    const char* charset = "utf-8";
    char fname[30];
    if (!item->appointment) return;

    // inline appointment request
    fprintf(f_output, "\n--%s\n", boundary);
    fprintf(f_output, "Content-Type: %s; method=\"%s\"; charset=\"%s\"\n\n", "text/calendar", method, charset);
    write_schedule_part_data(f_output, item, sender, method);
    fprintf(f_output, "\n");

    // attachment appointment request
    snprintf(fname, sizeof(fname), "i%" PRIu64 ".ics", item->block_id);
    fprintf(f_output, "\n--%s\n", boundary);
    fprintf(f_output, "Content-Type: %s; charset=\"%s\"; name=\"%s\"\n", "text/calendar", "utf-8", fname);
    fprintf(f_output, "Content-Disposition: attachment; filename=\"%s\"\n\n", fname);
    write_schedule_part_data(f_output, item, sender, method);
    fprintf(f_output, "\n");
}


void write_normal_email(FILE* f_output, char f_name[], pst_item* item, int current_mode, int mode_MH, pst_file* pst, int save_rtf, int embedding, char** extra_mime_headers)
{
    char boundary[60];
    char altboundary[66];
    char *altboundaryp = NULL;
    char body_charset[30];
    char buffer_charset[30];
    char body_report[60];
    char sender[60];
    int  sender_known = 0;
    char *temp = NULL;
    time_t em_time;
    char *c_time = NULL;
    char *headers = NULL;
    int has_from, has_subject, has_to, has_cc, has_date, has_msgid;
    has_from = has_subject = has_to = has_cc = has_date = has_msgid = 0;
    DEBUG_ENT("write_normal_email");

    pst_convert_utf8_null(item, &item->email->header);
    DEBUG_INFO(("PST headers\n%s\n", item->email->header.str));
    DEBUG_INFO(("Extra MIME headers\n%s\n", *extra_mime_headers));
    headers = valid_headers(item->email->header.str) ? item->email->header.str :
              valid_headers(*extra_mime_headers)     ? *extra_mime_headers     :
              NULL;

    // setup default body character set and report type
    strncpy(body_charset, pst_default_charset(item, sizeof(buffer_charset), buffer_charset), sizeof(body_charset));
    body_charset[sizeof(body_charset)-1] = '\0';
    strncpy(body_report, "delivery-status", sizeof(body_report));
    body_report[sizeof(body_report)-1] = '\0';

    // setup default sender
    pst_convert_utf8(item, &item->email->sender_address);
    if (item->email->sender_address.str && strchr(item->email->sender_address.str, '@')) {
        temp = item->email->sender_address.str;
        sender_known = 1;
    }
    else {
        temp = "MAILER-DAEMON";
    }
    strncpy(sender, temp, sizeof(sender));
    sender[sizeof(sender)-1] = '\0';

    // convert the sent date if it exists, or set it to a fixed date
    if (item->email->sent_date) {
        em_time = pst_fileTimeToUnixTime(item->email->sent_date);
        c_time = ctime(&em_time);
        if (c_time)
            c_time[strlen(c_time)-1] = '\0'; //remove end \n
    }
    if (!c_time)
        c_time = "Thu Jan 1 00:00:00 1970";

    // create our MIME boundaries here.
    snprintf(boundary, sizeof(boundary), "--boundary-LibPST-iamunique-%" PRIu64 "_-_-", item->block_id);
    snprintf(altboundary, sizeof(altboundary), "alt-%s", boundary);

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
            // but only for the outermost message
            if (!*extra_mime_headers) *extra_mime_headers = temp+2;
            DEBUG_INFO(("Found extra mime headers\n%s\n", temp+2));
        }

        // Check if the headers have all the necessary fields
        header_has_field(headers, "\nFrom:",        &has_from);
        header_has_field(headers, "\nTo:",          &has_to);
        header_has_field(headers, "\nSubject:",     &has_subject);
        header_has_field(headers, "\nDate:",        &has_date);
        header_has_field(headers, "\nCC:",          &has_cc);
        header_has_field(headers, "\nMessage-Id:",  &has_msgid);

        // look for charset and report-type in Content-Type header
        t = header_get_field(headers, "\nContent-Type:");
        header_get_subfield(t, "charset", body_charset, sizeof(body_charset));
        header_get_subfield(t, "report-type", body_report, sizeof(body_report));

        // derive a proper sender email address
        if (!sender_known) {
            t = header_get_field(headers, "\nFrom:");
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
        header_strip_field(headers, "\nMIME-Version:");
        header_strip_field(headers, "\nContent-Type:");
        header_strip_field(headers, "\nContent-Transfer-Encoding:");
        header_strip_field(headers, "\nContent-class:");
        header_strip_field(headers, "\nX-MimeOLE:");
        header_strip_field(headers, "\nX-From_:");
    }

    DEBUG_INFO(("About to print Header\n"));

    if (item && item->subject.str) {
        pst_convert_utf8(item, &item->subject);
        DEBUG_INFO(("item->subject = %s\n", item->subject.str));
    }

    if (mode != MODE_SEPARATE) {
        // most modes need this separator line.
        // procmail produces this separator without the quotes around the
        // sender email address, but apparently some Mac email client needs
        // those quotes, and they don't seem to cause problems for anyone else.
        char *quo = (embedding) ? ">" : "";
        fprintf(f_output, "%sFrom \"%s\" %s\n", quo, sender, c_time);
    }

    // print the supplied email headers
    if (headers) {
        int len = strlen(headers);
        if (len > 0) {
            fprintf(f_output, "%s", headers);
            // make sure the headers end with a \n
            if (headers[len-1] != '\n') fprintf(f_output, "\n");
            //char *h = headers;
            //while (*h) {
            //    char *e = strchr(h, '\n');
            //    int   d = 1;    // normally e points to trailing \n
            //    if (!e) {
            //        e = h + strlen(h);  // e points to trailing null
            //        d = 0;
            //    }
            //    // we could do rfc2047 encoding here if needed
            //    fprintf(f_output, "%.*s\n", (int)(e-h), h);
            //    h = e + d;
            //}
        }
    }

    // record read status
    if ((item->flags & PST_FLAG_READ) == PST_FLAG_READ) {
        fprintf(f_output, "Status: RO\n");
    }

    // create required header fields that are not already written

    if (!has_from) {
        if (item->email->outlook_sender_name.str){
            pst_rfc2047(item, &item->email->outlook_sender_name, 1);
            fprintf(f_output, "From: %s <%s>\n", item->email->outlook_sender_name.str, sender);
        } else {
            fprintf(f_output, "From: <%s>\n", sender);
        }
    }

    if (!has_subject) {
        if (item->subject.str) {
            pst_rfc2047(item, &item->subject, 0);
            fprintf(f_output, "Subject: %s\n", item->subject.str);
        } else {
            fprintf(f_output, "Subject: \n");
        }
    }

    if (!has_to && item->email->sentto_address.str) {
        pst_rfc2047(item, &item->email->sentto_address, 0);
        fprintf(f_output, "To: %s\n", item->email->sentto_address.str);
    }

    if (!has_cc && item->email->cc_address.str) {
        pst_rfc2047(item, &item->email->cc_address, 0);
        fprintf(f_output, "Cc: %s\n", item->email->cc_address.str);
    }

    if (!has_date && item->email->sent_date) {
        char c_time[C_TIME_SIZE];
        struct tm stm;
        gmtime_r(&em_time, &stm);
        strftime(c_time, C_TIME_SIZE, "%a, %d %b %Y %H:%M:%S %z", &stm);
        fprintf(f_output, "Date: %s\n", c_time);
    }

    if (!has_msgid && item->email->messageid.str) {
        pst_convert_utf8(item, &item->email->messageid);
        fprintf(f_output, "Message-Id: %s\n", item->email->messageid.str);
    }

    // add forensic headers to capture some .pst stuff that is not really
    // needed or used by mail clients
    pst_convert_utf8_null(item, &item->email->sender_address);
    if (item->email->sender_address.str && !strchr(item->email->sender_address.str, '@')
                                        && strcmp(item->email->sender_address.str, ".")
                                        && (strlen(item->email->sender_address.str) > 0)) {
        fprintf(f_output, "X-libpst-forensic-sender: %s\n", item->email->sender_address.str);
    }

    if (item->email->bcc_address.str) {
        pst_convert_utf8(item, &item->email->bcc_address);
        fprintf(f_output, "X-libpst-forensic-bcc: %s\n", item->email->bcc_address.str);
    }

    // add our own mime headers
    fprintf(f_output, "MIME-Version: 1.0\n");
    if (item->type == PST_TYPE_REPORT) {
        // multipart/report for DSN/MDN reports
        fprintf(f_output, "Content-Type: multipart/report; report-type=%s;\n\tboundary=\"%s\"\n", body_report, boundary);
    }
    else {
        fprintf(f_output, "Content-Type: multipart/mixed;\n\tboundary=\"%s\"\n", boundary);
    }
    fprintf(f_output, "\n");    // end of headers, start of body

    // now dump the body parts
    if ((item->type == PST_TYPE_REPORT) && (item->email->report_text.str)) {
        write_body_part(f_output, &item->email->report_text, "text/plain", body_charset, boundary, pst);
        fprintf(f_output, "\n");
    }

    if (item->body.str && item->email->htmlbody.str) {
        // start the nested alternative part
        fprintf(f_output, "\n--%s\n", boundary);
        fprintf(f_output, "Content-Type: multipart/alternative;\n\tboundary=\"%s\"\n", altboundary);
        altboundaryp = altboundary;
    }
    else {
        altboundaryp = boundary;
    }

    if (item->body.str) {
        write_body_part(f_output, &item->body, "text/plain", body_charset, altboundaryp, pst);
    }

    if (item->email->htmlbody.str) {
        find_html_charset(item->email->htmlbody.str, body_charset, sizeof(body_charset));
        write_body_part(f_output, &item->email->htmlbody, "text/html", body_charset, altboundaryp, pst);
    }

    if (item->body.str && item->email->htmlbody.str) {
        // end the nested alternative part
        fprintf(f_output, "\n--%s--\n", altboundary);
    }

    if (item->email->rtf_compressed.data && save_rtf) {
        pst_item_attach* attach = (pst_item_attach*)pst_malloc(sizeof(pst_item_attach));
        DEBUG_INFO(("Adding RTF body as attachment\n"));
        memset(attach, 0, sizeof(pst_item_attach));
        attach->next = item->attach;
        item->attach = attach;
        attach->data.data         = pst_lzfu_decompress(item->email->rtf_compressed.data, item->email->rtf_compressed.size, &attach->data.size);
        attach->filename2.str     = strdup(RTF_ATTACH_NAME);
        attach->filename2.is_utf8 = 1;
        attach->mimetype.str      = strdup(RTF_ATTACH_TYPE);
        attach->mimetype.is_utf8  = 1;
    }

    if (item->email->encrypted_body.data) {
        pst_item_attach* attach = (pst_item_attach*)pst_malloc(sizeof(pst_item_attach));
        DEBUG_INFO(("Adding encrypted text body as attachment\n"));
        memset(attach, 0, sizeof(pst_item_attach));
        attach->next = item->attach;
        item->attach = attach;
        attach->data.data = item->email->encrypted_body.data;
        attach->data.size = item->email->encrypted_body.size;
        item->email->encrypted_body.data = NULL;
    }

    if (item->email->encrypted_htmlbody.data) {
        pst_item_attach* attach = (pst_item_attach*)pst_malloc(sizeof(pst_item_attach));
        DEBUG_INFO(("Adding encrypted HTML body as attachment\n"));
        memset(attach, 0, sizeof(pst_item_attach));
        attach->next = item->attach;
        item->attach = attach;
        attach->data.data = item->email->encrypted_htmlbody.data;
        attach->data.size = item->email->encrypted_htmlbody.size;
        item->email->encrypted_htmlbody.data = NULL;
    }

    if (item->type == PST_TYPE_SCHEDULE) {
        write_schedule_part(f_output, item, sender, boundary);
    }

    // other attachments
    {
        pst_item_attach* attach;
        int attach_num = 0;
        for (attach = item->attach; attach; attach = attach->next) {
            pst_convert_utf8_null(item, &attach->filename1);
            pst_convert_utf8_null(item, &attach->filename2);
            pst_convert_utf8_null(item, &attach->mimetype);
            DEBUG_INFO(("Attempting Attachment encoding\n"));
            if (attach->method == PST_ATTACH_EMBEDDED) {
                DEBUG_INFO(("have an embedded rfc822 message attachment\n"));
                if (attach->mimetype.str) {
                    DEBUG_INFO(("which already has a mime-type of %s\n", attach->mimetype.str));
                    free(attach->mimetype.str);
                }
                attach->mimetype.str = strdup(RFC822);
                attach->mimetype.is_utf8 = 1;
                find_rfc822_headers(extra_mime_headers);
                write_embedded_message(f_output, attach, boundary, pst, save_rtf, extra_mime_headers);
            }
            else if (attach->data.data || attach->i_id) {
                if (acceptable_ext(attach)) {
                    if (current_mode == MODE_SEPARATE && !mode_MH)
                        write_separate_attachment(f_name, attach, ++attach_num, pst);
                    else
                        write_inline_attachment(f_output, attach, boundary, pst);
                }
            }
        }
    }

    fprintf(f_output, "\n--%s--\n\n", boundary);
    DEBUG_RET();
}


void write_vcard(FILE* f_output, pst_item* item, pst_item_contact* contact, char comment[])
{
    char*  result = NULL;
    size_t resultlen = 0;
    char   time_buffer[30];
    // We can only call rfc escape once per printf, since the second call
    // may free the buffer returned by the first call.
    // I had tried to place those into a single printf - Carl.

    DEBUG_ENT("write_vcard");

    // make everything utf8
    pst_convert_utf8_null(item, &contact->fullname);
    pst_convert_utf8_null(item, &contact->surname);
    pst_convert_utf8_null(item, &contact->first_name);
    pst_convert_utf8_null(item, &contact->middle_name);
    pst_convert_utf8_null(item, &contact->display_name_prefix);
    pst_convert_utf8_null(item, &contact->suffix);
    pst_convert_utf8_null(item, &contact->nickname);
    pst_convert_utf8_null(item, &contact->address1);
    pst_convert_utf8_null(item, &contact->address2);
    pst_convert_utf8_null(item, &contact->address3);
    pst_convert_utf8_null(item, &contact->home_po_box);
    pst_convert_utf8_null(item, &contact->home_street);
    pst_convert_utf8_null(item, &contact->home_city);
    pst_convert_utf8_null(item, &contact->home_state);
    pst_convert_utf8_null(item, &contact->home_postal_code);
    pst_convert_utf8_null(item, &contact->home_country);
    pst_convert_utf8_null(item, &contact->home_address);
    pst_convert_utf8_null(item, &contact->business_po_box);
    pst_convert_utf8_null(item, &contact->business_street);
    pst_convert_utf8_null(item, &contact->business_city);
    pst_convert_utf8_null(item, &contact->business_state);
    pst_convert_utf8_null(item, &contact->business_postal_code);
    pst_convert_utf8_null(item, &contact->business_country);
    pst_convert_utf8_null(item, &contact->business_address);
    pst_convert_utf8_null(item, &contact->other_po_box);
    pst_convert_utf8_null(item, &contact->other_street);
    pst_convert_utf8_null(item, &contact->other_city);
    pst_convert_utf8_null(item, &contact->other_state);
    pst_convert_utf8_null(item, &contact->other_postal_code);
    pst_convert_utf8_null(item, &contact->other_country);
    pst_convert_utf8_null(item, &contact->other_address);
    pst_convert_utf8_null(item, &contact->business_fax);
    pst_convert_utf8_null(item, &contact->business_phone);
    pst_convert_utf8_null(item, &contact->business_phone2);
    pst_convert_utf8_null(item, &contact->car_phone);
    pst_convert_utf8_null(item, &contact->home_fax);
    pst_convert_utf8_null(item, &contact->home_phone);
    pst_convert_utf8_null(item, &contact->home_phone2);
    pst_convert_utf8_null(item, &contact->isdn_phone);
    pst_convert_utf8_null(item, &contact->mobile_phone);
    pst_convert_utf8_null(item, &contact->other_phone);
    pst_convert_utf8_null(item, &contact->pager_phone);
    pst_convert_utf8_null(item, &contact->primary_fax);
    pst_convert_utf8_null(item, &contact->primary_phone);
    pst_convert_utf8_null(item, &contact->radio_phone);
    pst_convert_utf8_null(item, &contact->telex);
    pst_convert_utf8_null(item, &contact->job_title);
    pst_convert_utf8_null(item, &contact->profession);
    pst_convert_utf8_null(item, &contact->assistant_name);
    pst_convert_utf8_null(item, &contact->assistant_phone);
    pst_convert_utf8_null(item, &contact->company_name);
    pst_convert_utf8_null(item, &item->body);

    // the specification I am following is (hopefully) RFC2426 vCard Mime Directory Profile
    fprintf(f_output, "BEGIN:VCARD\n");
    fprintf(f_output, "FN:%s\n", pst_rfc2426_escape(contact->fullname.str, &result, &resultlen));

    //fprintf(f_output, "N:%s;%s;%s;%s;%s\n",
    fprintf(f_output, "N:%s;", (!contact->surname.str)             ? "" : pst_rfc2426_escape(contact->surname.str, &result, &resultlen));
    fprintf(f_output, "%s;",   (!contact->first_name.str)          ? "" : pst_rfc2426_escape(contact->first_name.str, &result, &resultlen));
    fprintf(f_output, "%s;",   (!contact->middle_name.str)         ? "" : pst_rfc2426_escape(contact->middle_name.str, &result, &resultlen));
    fprintf(f_output, "%s;",   (!contact->display_name_prefix.str) ? "" : pst_rfc2426_escape(contact->display_name_prefix.str, &result, &resultlen));
    fprintf(f_output, "%s\n",  (!contact->suffix.str)              ? "" : pst_rfc2426_escape(contact->suffix.str, &result, &resultlen));

    if (contact->nickname.str)
        fprintf(f_output, "NICKNAME:%s\n", pst_rfc2426_escape(contact->nickname.str, &result, &resultlen));
    if (contact->address1.str)
        fprintf(f_output, "EMAIL:%s\n", pst_rfc2426_escape(contact->address1.str, &result, &resultlen));
    if (contact->address2.str)
        fprintf(f_output, "EMAIL:%s\n", pst_rfc2426_escape(contact->address2.str, &result, &resultlen));
    if (contact->address3.str)
        fprintf(f_output, "EMAIL:%s\n", pst_rfc2426_escape(contact->address3.str, &result, &resultlen));
    if (contact->birthday)
        fprintf(f_output, "BDAY:%s\n", pst_rfc2425_datetime_format(contact->birthday, sizeof(time_buffer), time_buffer));

    if (contact->home_address.str) {
        //fprintf(f_output, "ADR;TYPE=home:%s;%s;%s;%s;%s;%s;%s\n",
        fprintf(f_output, "ADR;TYPE=home:%s;",  (!contact->home_po_box.str)      ? "" : pst_rfc2426_escape(contact->home_po_box.str, &result, &resultlen));
        fprintf(f_output, "%s;",                ""); // extended Address
        fprintf(f_output, "%s;",                (!contact->home_street.str)      ? "" : pst_rfc2426_escape(contact->home_street.str, &result, &resultlen));
        fprintf(f_output, "%s;",                (!contact->home_city.str)        ? "" : pst_rfc2426_escape(contact->home_city.str, &result, &resultlen));
        fprintf(f_output, "%s;",                (!contact->home_state.str)       ? "" : pst_rfc2426_escape(contact->home_state.str, &result, &resultlen));
        fprintf(f_output, "%s;",                (!contact->home_postal_code.str) ? "" : pst_rfc2426_escape(contact->home_postal_code.str, &result, &resultlen));
        fprintf(f_output, "%s\n",               (!contact->home_country.str)     ? "" : pst_rfc2426_escape(contact->home_country.str, &result, &resultlen));
        fprintf(f_output, "LABEL;TYPE=home:%s\n", pst_rfc2426_escape(contact->home_address.str, &result, &resultlen));
    }

    if (contact->business_address.str) {
        //fprintf(f_output, "ADR;TYPE=work:%s;%s;%s;%s;%s;%s;%s\n",
        fprintf(f_output, "ADR;TYPE=work:%s;",  (!contact->business_po_box.str)      ? "" : pst_rfc2426_escape(contact->business_po_box.str, &result, &resultlen));
        fprintf(f_output, "%s;",                ""); // extended Address
        fprintf(f_output, "%s;",                (!contact->business_street.str)      ? "" : pst_rfc2426_escape(contact->business_street.str, &result, &resultlen));
        fprintf(f_output, "%s;",                (!contact->business_city.str)        ? "" : pst_rfc2426_escape(contact->business_city.str, &result, &resultlen));
        fprintf(f_output, "%s;",                (!contact->business_state.str)       ? "" : pst_rfc2426_escape(contact->business_state.str, &result, &resultlen));
        fprintf(f_output, "%s;",                (!contact->business_postal_code.str) ? "" : pst_rfc2426_escape(contact->business_postal_code.str, &result, &resultlen));
        fprintf(f_output, "%s\n",               (!contact->business_country.str)     ? "" : pst_rfc2426_escape(contact->business_country.str, &result, &resultlen));
        fprintf(f_output, "LABEL;TYPE=work:%s\n", pst_rfc2426_escape(contact->business_address.str, &result, &resultlen));
    }

    if (contact->other_address.str) {
        //fprintf(f_output, "ADR;TYPE=postal:%s;%s;%s;%s;%s;%s;%s\n",
        fprintf(f_output, "ADR;TYPE=postal:%s;",(!contact->other_po_box.str)       ? "" : pst_rfc2426_escape(contact->other_po_box.str, &result, &resultlen));
        fprintf(f_output, "%s;",                ""); // extended Address
        fprintf(f_output, "%s;",                (!contact->other_street.str)       ? "" : pst_rfc2426_escape(contact->other_street.str, &result, &resultlen));
        fprintf(f_output, "%s;",                (!contact->other_city.str)         ? "" : pst_rfc2426_escape(contact->other_city.str, &result, &resultlen));
        fprintf(f_output, "%s;",                (!contact->other_state.str)        ? "" : pst_rfc2426_escape(contact->other_state.str, &result, &resultlen));
        fprintf(f_output, "%s;",                (!contact->other_postal_code.str)  ? "" : pst_rfc2426_escape(contact->other_postal_code.str, &result, &resultlen));
        fprintf(f_output, "%s\n",               (!contact->other_country.str)      ? "" : pst_rfc2426_escape(contact->other_country.str, &result, &resultlen));
        fprintf(f_output, "LABEL;TYPE=postal:%s\n", pst_rfc2426_escape(contact->other_address.str, &result, &resultlen));
    }

    if (contact->business_fax.str)      fprintf(f_output, "TEL;TYPE=work,fax:%s\n",         pst_rfc2426_escape(contact->business_fax.str, &result, &resultlen));
    if (contact->business_phone.str)    fprintf(f_output, "TEL;TYPE=work,voice:%s\n",       pst_rfc2426_escape(contact->business_phone.str, &result, &resultlen));
    if (contact->business_phone2.str)   fprintf(f_output, "TEL;TYPE=work,voice:%s\n",       pst_rfc2426_escape(contact->business_phone2.str, &result, &resultlen));
    if (contact->car_phone.str)         fprintf(f_output, "TEL;TYPE=car,voice:%s\n",        pst_rfc2426_escape(contact->car_phone.str, &result, &resultlen));
    if (contact->home_fax.str)          fprintf(f_output, "TEL;TYPE=home,fax:%s\n",         pst_rfc2426_escape(contact->home_fax.str, &result, &resultlen));
    if (contact->home_phone.str)        fprintf(f_output, "TEL;TYPE=home,voice:%s\n",       pst_rfc2426_escape(contact->home_phone.str, &result, &resultlen));
    if (contact->home_phone2.str)       fprintf(f_output, "TEL;TYPE=home,voice:%s\n",       pst_rfc2426_escape(contact->home_phone2.str, &result, &resultlen));
    if (contact->isdn_phone.str)        fprintf(f_output, "TEL;TYPE=isdn:%s\n",             pst_rfc2426_escape(contact->isdn_phone.str, &result, &resultlen));
    if (contact->mobile_phone.str)      fprintf(f_output, "TEL;TYPE=cell,voice:%s\n",       pst_rfc2426_escape(contact->mobile_phone.str, &result, &resultlen));
    if (contact->other_phone.str)       fprintf(f_output, "TEL;TYPE=msg:%s\n",              pst_rfc2426_escape(contact->other_phone.str, &result, &resultlen));
    if (contact->pager_phone.str)       fprintf(f_output, "TEL;TYPE=pager:%s\n",            pst_rfc2426_escape(contact->pager_phone.str, &result, &resultlen));
    if (contact->primary_fax.str)       fprintf(f_output, "TEL;TYPE=fax,pref:%s\n",         pst_rfc2426_escape(contact->primary_fax.str, &result, &resultlen));
    if (contact->primary_phone.str)     fprintf(f_output, "TEL;TYPE=phone,pref:%s\n",       pst_rfc2426_escape(contact->primary_phone.str, &result, &resultlen));
    if (contact->radio_phone.str)       fprintf(f_output, "TEL;TYPE=pcs:%s\n",              pst_rfc2426_escape(contact->radio_phone.str, &result, &resultlen));
    if (contact->telex.str)             fprintf(f_output, "TEL;TYPE=bbs:%s\n",              pst_rfc2426_escape(contact->telex.str, &result, &resultlen));
    if (contact->job_title.str)         fprintf(f_output, "TITLE:%s\n",                     pst_rfc2426_escape(contact->job_title.str, &result, &resultlen));
    if (contact->profession.str)        fprintf(f_output, "ROLE:%s\n",                      pst_rfc2426_escape(contact->profession.str, &result, &resultlen));
    if (contact->assistant_name.str || contact->assistant_phone.str) {
        fprintf(f_output, "AGENT:BEGIN:VCARD\n");
        if (contact->assistant_name.str)    fprintf(f_output, "FN:%s\n",                    pst_rfc2426_escape(contact->assistant_name.str, &result, &resultlen));
        if (contact->assistant_phone.str)   fprintf(f_output, "TEL:%s\n",                   pst_rfc2426_escape(contact->assistant_phone.str, &result, &resultlen));
    }
    if (contact->company_name.str)      fprintf(f_output, "ORG:%s\n",                       pst_rfc2426_escape(contact->company_name.str, &result, &resultlen));
    if (comment)                        fprintf(f_output, "NOTE:%s\n",                      pst_rfc2426_escape(comment, &result, &resultlen));
    if (item->body.str)                 fprintf(f_output, "NOTE:%s\n",                      pst_rfc2426_escape(item->body.str, &result, &resultlen));

    write_extra_categories(f_output, item);

    fprintf(f_output, "VERSION:3.0\n");
    fprintf(f_output, "END:VCARD\n\n");
    if (result) free(result);
    DEBUG_RET();
}


/**
 * write extra vcard or vcalendar categories from the extra keywords fields
 *
 * @param f_output open file pointer
 * @param item     pst item containing the keywords
 * @return         true if we write a categories line
 */
int write_extra_categories(FILE* f_output, pst_item* item)
{
    char*  result = NULL;
    size_t resultlen = 0;
    pst_item_extra_field *ef = item->extra_fields;
    const char *fmt = "CATEGORIES:%s";
    int category_started = 0;
    while (ef) {
        if (strcmp(ef->field_name, "Keywords") == 0) {
            fprintf(f_output, fmt, pst_rfc2426_escape(ef->value, &result, &resultlen));
            fmt = ", %s";
            category_started = 1;
        }
        ef = ef->next;
    }
    if (category_started) fprintf(f_output, "\n");
    if (result) free(result);
    return category_started;
}


void write_journal(FILE* f_output, pst_item* item)
{
    char*  result = NULL;
    size_t resultlen = 0;
    char   time_buffer[30];
    pst_item_journal* journal = item->journal;

    // make everything utf8
    pst_convert_utf8_null(item, &item->subject);
    pst_convert_utf8_null(item, &item->body);

    fprintf(f_output, "BEGIN:VJOURNAL\n");
    // FIXME: use the attendee/owner critical change property for DTSTAMP
    // https://learn.microsoft.com/en-us/openspecs/exchange_server_protocols/ms-oxcical/4b93e7b6-142e-4f0c-ac08-1505a6fa0199
    // https://learn.microsoft.com/en-us/office/client-developer/outlook/mapi/pidlidattendeecriticalchange-canonical-property
    // https://learn.microsoft.com/en-us/office/client-developer/outlook/mapi/pidlidownercriticalchange-canonical-property
    if (item->create_date)
        fprintf(f_output, "DTSTAMP:%s\n",                 pst_rfc2445_datetime_format(item->create_date, sizeof(time_buffer), time_buffer));
    else
        fprintf(f_output, "DTSTAMP:%s\n",                 pst_rfc2445_datetime_format_now(sizeof(time_buffer), time_buffer));
    if (item->create_date)
        fprintf(f_output, "CREATED:%s\n",                 pst_rfc2445_datetime_format(item->create_date, sizeof(time_buffer), time_buffer));
    if (item->modify_date)
        fprintf(f_output, "LAST-MOD:%s\n",                pst_rfc2445_datetime_format(item->modify_date, sizeof(time_buffer), time_buffer));
    if (item->subject.str)
        fprintf(f_output, "SUMMARY:%s\n",                 pst_rfc2426_escape(item->subject.str, &result, &resultlen));
    if (item->body.str)
        fprintf(f_output, "DESCRIPTION:%s\n",             pst_rfc2426_escape(item->body.str, &result, &resultlen));
    if (journal && journal->start)
        fprintf(f_output, "DTSTART;VALUE=DATE-TIME:%s\n", pst_rfc2445_datetime_format(journal->start, sizeof(time_buffer), time_buffer));
    fprintf(f_output, "END:VJOURNAL\n");
    if (result) free(result);
}


void write_appointment(FILE* f_output, pst_item* item)
{
    char*  result = NULL;
    size_t resultlen = 0;
    char   time_buffer[30];
    pst_item_appointment* appointment = item->appointment;

    // make everything utf8
    pst_convert_utf8_null(item, &item->subject);
    pst_convert_utf8_null(item, &item->body);
    pst_convert_utf8_null(item, &appointment->location);

    fprintf(f_output, "UID:%#" PRIx64 "\n", item->block_id);
    // FIXME: use the attendee/owner critical change property for DTSTAMP
    // https://learn.microsoft.com/en-us/openspecs/exchange_server_protocols/ms-oxcical/4b93e7b6-142e-4f0c-ac08-1505a6fa0199
    // https://learn.microsoft.com/en-us/office/client-developer/outlook/mapi/pidlidattendeecriticalchange-canonical-property
    // https://learn.microsoft.com/en-us/office/client-developer/outlook/mapi/pidlidownercriticalchange-canonical-property
    if (item->create_date)
        fprintf(f_output, "DTSTAMP:%s\n",                 pst_rfc2445_datetime_format(item->create_date, sizeof(time_buffer), time_buffer));
    else
        fprintf(f_output, "DTSTAMP:%s\n",                 pst_rfc2445_datetime_format_now(sizeof(time_buffer), time_buffer));
    if (item->create_date)
        fprintf(f_output, "CREATED:%s\n",                 pst_rfc2445_datetime_format(item->create_date, sizeof(time_buffer), time_buffer));
    if (item->modify_date)
        fprintf(f_output, "LAST-MOD:%s\n",                pst_rfc2445_datetime_format(item->modify_date, sizeof(time_buffer), time_buffer));
    if (item->subject.str)
        fprintf(f_output, "SUMMARY:%s\n",                 pst_rfc2426_escape(item->subject.str, &result, &resultlen));
    if (item->body.str)
        fprintf(f_output, "DESCRIPTION:%s\n",             pst_rfc2426_escape(item->body.str, &result, &resultlen));
    if (appointment && appointment->start)
        fprintf(f_output, "DTSTART;VALUE=DATE-TIME:%s\n", pst_rfc2445_datetime_format(appointment->start, sizeof(time_buffer), time_buffer));
    if (appointment && appointment->end)
        fprintf(f_output, "DTEND;VALUE=DATE-TIME:%s\n",   pst_rfc2445_datetime_format(appointment->end, sizeof(time_buffer), time_buffer));
    if (appointment && appointment->location.str)
        fprintf(f_output, "LOCATION:%s\n",                pst_rfc2426_escape(appointment->location.str, &result, &resultlen));
    if (appointment) {
        switch (appointment->showas) {
            case PST_FREEBUSY_TENTATIVE:
                fprintf(f_output, "STATUS:TENTATIVE\n");
                break;
            case PST_FREEBUSY_FREE:
                // mark as transparent and as confirmed
                fprintf(f_output, "TRANSP:TRANSPARENT\n");
                fprintf(f_output, "STATUS:CONFIRMED\n");
                break;
            case PST_FREEBUSY_BUSY:
            case PST_FREEBUSY_OUT_OF_OFFICE:
                fprintf(f_output, "STATUS:CONFIRMED\n");
                break;
        }
        if (appointment->is_recurring) {
            const char* rules[] = {"DAILY", "WEEKLY", "MONTHLY", "YEARLY"};
            const char* days[]  = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
            pst_recurrence *rdata = pst_convert_recurrence(appointment);
            fprintf(f_output, "RRULE:FREQ=%s", rules[rdata->type]);
            if (rdata->count)       fprintf(f_output, ";COUNT=%" PRIu32,      rdata->count);
            if ((rdata->interval != 1) &&
                (rdata->interval))  fprintf(f_output, ";INTERVAL=%" PRIu32,   rdata->interval);
            if (rdata->dayofmonth)  fprintf(f_output, ";BYMONTHDAY=%" PRIu32, rdata->dayofmonth);
            if (rdata->monthofyear) fprintf(f_output, ";BYMONTH=%" PRIu32,    rdata->monthofyear);
            if (rdata->position)    fprintf(f_output, ";BYSETPOS=%" PRIu32,   rdata->position);
            if (rdata->bydaymask) {
                char byday[40];
                int  empty = 1;
                int i=0;
                memset(byday, 0, sizeof(byday));
                for (i=0; i<7; i++) {
                    int bit = 1 << i;
                    if (bit & rdata->bydaymask) {
                        char temp[49];
                        snprintf(temp, sizeof(temp), "%s%s%s", byday, (empty) ? ";BYDAY=" : ";", days[i]);
                        strcpy(byday, temp);
                        empty = 0;
                    }
                }
                fprintf(f_output, "%s", byday);
            }
            fprintf(f_output, "\n");
            pst_free_recurrence(rdata);
        }
        switch (appointment->label) {
            case PST_APP_LABEL_NONE:
                if (!write_extra_categories(f_output, item)) fprintf(f_output, "CATEGORIES:NONE\n");
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
        // ignore bogus alarms
        if (appointment->alarm && (appointment->alarm_minutes >= 0) && (appointment->alarm_minutes < 1440)) {
            fprintf(f_output, "BEGIN:VALARM\n");
            fprintf(f_output, "TRIGGER:-PT%" PRId32 "M\n", appointment->alarm_minutes);
            fprintf(f_output, "ACTION:DISPLAY\n");
            fprintf(f_output, "DESCRIPTION:Reminder\n");
            fprintf(f_output, "END:VALARM\n");
        }
    }
    fprintf(f_output, "END:VEVENT\n");
    if (result) free(result);
}


void create_enter_dir(struct file_ll* f, pst_item *item)
{
    memset(f, 0, sizeof(*f));
    f->stored_count = (item->folder) ? item->folder->item_count : 0;
    pst_convert_utf8(item, &item->file_as);
    f->dname = (char*) pst_malloc(strlen(item->file_as.str)+1);
    strcpy(f->dname, item->file_as.str);

    DEBUG_ENT("create_enter_dir");
    if (mode == MODE_KMAIL) {
        int32_t t;
        mk_kmail_dir(item->file_as.str);
        for (t=0; t<PST_TYPE_MAX; t++) {
            if (t == reduced_item_type(t)) {
                f->name[t] = (char*) pst_malloc(strlen(item->file_as.str)+strlen(OUTPUT_TEMPLATE)+30);
                sprintf(f->name[t], OUTPUT_TEMPLATE, item->file_as.str, item_type_to_name(t));
            }
        }
    } else if (mode == MODE_RECURSE) {
        int32_t t;
        mk_recurse_dir(item->file_as.str);
        for (t=0; t<PST_TYPE_MAX; t++) {
            if (t == reduced_item_type(t)) {
                f->name[t] = strdup(item_type_to_name(t));
            }
        }
        if (mode_thunder) {
            FILE *type_file = fopen(".type", "w");
            if (type_file) {
                fprintf(type_file, "%d\n", item->type);
                fclose(type_file);
            } else {
                DEBUG_WARN(("could not write .type file: %d\n", item->type));
            }
        }
    } else if (mode == MODE_SEPARATE) {
        // do similar stuff to recurse here.
        int32_t t;
        mk_separate_dir(item->file_as.str);
        for (t=0; t<PST_TYPE_MAX; t++) {
            if (t == reduced_item_type(t)) {
                f->name[t] = (char*) pst_malloc(file_name_len);
                memset(f->name[t], 0, file_name_len);
            }
        }
    } else {
        // MODE_NORMAL
        int32_t t;
        for (t=0; t<PST_TYPE_MAX; t++) {
            if (t == reduced_item_type(t)) {
                f->name[t] = (char*) pst_malloc(strlen(item->file_as.str)+strlen(OUTPUT_TEMPLATE)+30);
                sprintf(f->name[t], OUTPUT_TEMPLATE, item->file_as.str, item_type_to_name(t));
            }
        }
    }

    if (mode != MODE_SEPARATE) {
        int32_t t;
        for (t=0; t<PST_TYPE_MAX; t++) {
            if (f->name[t]) {
                if (!overwrite) {
                    int x = 0;
                    char *temp = (char*) pst_malloc (strlen(f->name[t])+10); //enough room for 10 digits

                    sprintf(temp, "%s", f->name[t]);
                    check_filename(temp);
                    while ((f->output[t] = fopen(temp, "r"))) {
                        DEBUG_INFO(("need to increase filename because one already exists with that name\n"));
                        x++;
                        sprintf(temp, "%s%08d", f->name[t], x);
                        DEBUG_INFO(("- bump file name and try \"%s\"\n", temp));
                        if (x == 99999999) {
                            DIE(("create_enter_dir: Why can I not create a folder %s? I have tried %i extensions...\n", f->name[t], x));
                        }
                        fclose(f->output[t]);
                    }
                    if (x > 0) { //then the f->name should change
                        free (f->name[t]);
                        f->name[t] = temp;
                    } else {
                        free(temp);
                    }
                }
                check_filename(f->name[t]);
                if (!(f->output[t] = fopen(f->name[t], "w"))) {
                    DIE(("create_enter_dir: Could not open file \"%s\" for write\n", f->name[t]));
                }
                DEBUG_INFO(("f->name = %s\nitem->folder_name = %s\n", f->name[t], item->file_as.str));
            }
        }
    }
    DEBUG_RET();
}


void close_enter_dir(struct file_ll *f)
{
    int32_t t;
    DEBUG_INFO(("processed item count for folder %s is %i, skipped %i, total %i \n",
                f->dname, f->item_count, f->skip_count, f->stored_count));
    if (output_mode != OUTPUT_QUIET) {
        pst_debug_lock();
            printf("\t\"%s\" - %" PRIi32 " items done, %" PRIi32 " items skipped.\n", f->dname, f->item_count, f->skip_count);
            fflush(stdout);
        pst_debug_unlock();
    }
    for (t=0; t<PST_TYPE_MAX; t++) {
        if (f->output[t]) {
            if (mode == MODE_SEPARATE) DEBUG_WARN(("close_enter_dir finds open separate file\n"));
            fclose(f->output[t]);
            f->output[t] = NULL;
        }
        if (f->name[t]) {
            struct stat st;
            stat(f->name[t], &st);
            if (!st.st_size) {
                DEBUG_WARN(("removing empty output file %s\n", f->name[t]));
                remove(f->name[t]);
            }
            free(f->name[t]);
            f->name[t] = NULL;
        }
    }
    free(f->dname);

    if (mode == MODE_KMAIL)
        close_kmail_dir();
    else if (mode == MODE_RECURSE) {
        if (mode_thunder) {
            FILE *type_file = fopen(".size", "w");
            if (type_file) {
                fprintf(type_file, "%" PRIi32 " %" PRIi32 "\n", f->item_count, f->stored_count);
                fclose(type_file);
            } else {
                DEBUG_WARN(("could not write .size file: %" PRIi32 " %" PRIi32 "\n", f->item_count, f->stored_count));
            }
        }
        close_recurse_dir();
    } else if (mode == MODE_SEPARATE)
        close_separate_dir();
}

