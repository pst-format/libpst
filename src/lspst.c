/***
 * lspst.c
 * Part of the LibPST project
 * Author: Joe Nahmias <joe@nahmias.net>
 * Based on readpst.c by by David Smith <dave.s@earthcorp.com>
 *
 */

#include "define.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#include "libpst.h"
#include "timeconv.h"

struct file_ll {
    char *dname;
    int32_t stored_count;
    int32_t email_count;
    int32_t skip_count;
    int32_t type;
};


void canonicalize_filename(char *fname);
void debug_print(char *fmt, ...);
int  usage(char *prog_name);
int  version();

// global settings
pst_file pstfile;


void create_enter_dir(struct file_ll* f, pst_item *item)
{
    f->email_count  = 0;
    f->skip_count   = 0;
    f->type         = item->type;
    f->stored_count = (item->folder) ? item->folder->email_count : 0;
    f->dname        = (char*) xmalloc(strlen(item->file_as)+1);
    strcpy(f->dname, item->file_as);
}


void close_enter_dir(struct file_ll *f)
{
    free(f->dname);
}


void process(pst_item *outeritem, pst_desc_ll *d_ptr)
{
    struct file_ll ff;
    pst_item *item = NULL;

    DEBUG_ENT("process");
    memset(&ff, 0, sizeof(ff));
    create_enter_dir(&ff, outeritem);

    while (d_ptr) {
        DEBUG_MAIN(("main: New item record, d_ptr = %p.\n", d_ptr));
        if (!d_ptr->desc) {
            DEBUG_WARN(("main: ERROR ?? item's desc record is NULL\n"));
            ff.skip_count++;
        }
        else {
            DEBUG_MAIN(("main: Desc Email ID %x [d_ptr->id = %x]\n", d_ptr->desc->id, d_ptr->id));

            item = pst_parse_item(&pstfile, d_ptr);
            DEBUG_MAIN(("main: About to process item @ %p.\n", item));
            if (item) {
                if (item->message_store) {
                    // there should only be one message_store, and we have already done it
                    DIE(("main: A second message_store has been found. Sorry, this must be an error.\n"));
                }

                if (item->folder && d_ptr->child) {
                    // if this is a folder, we want to recurse into it
                    printf("Folder \"%s\"\n", item->file_as);
                    process(item, d_ptr->child);

                } else if (item->contact && (item->type == PST_TYPE_CONTACT)) {
                    // Process Contact item
                    if (ff.type != PST_TYPE_CONTACT) {
                        DEBUG_MAIN(("main: I have a contact, but the folder isn't a contacts folder. Processing anyway\n"));
                    }
                    printf("Contact");
                    if (item->contact->fullname)
                        printf("\t%s", pst_rfc2426_escape(item->contact->fullname));
                    printf("\n");

                } else if (item->email && (item->type == PST_TYPE_NOTE || item->type == PST_TYPE_REPORT)) {
                    // Process Email item
                    if ((ff.type != PST_TYPE_NOTE) && (ff.type != PST_TYPE_REPORT)) {
                        DEBUG_MAIN(("main: I have an email, but the folder isn't an email folder. Processing anyway\n"));
                    }
                    printf("Email");
                    if (item->email->outlook_sender_name)
                        printf("\tFrom: %s", item->email->outlook_sender_name);
                    if (item->email->subject && item->email->subject->subj)
                        printf("\tSubject: %s", item->email->subject->subj);
                    printf("\n");

                } else if (item->journal && (item->type == PST_TYPE_JOURNAL)) {
                    // Process Journal item
                    if (ff.type != PST_TYPE_JOURNAL) {
                        DEBUG_MAIN(("main: I have a journal entry, but folder isn't specified as a journal type. Processing...\n"));
                    }
                    if (item->email && item->email->subject && item->email->subject->subj)
                        printf("Journal\t%s\n", pst_rfc2426_escape(item->email->subject->subj));

                } else if (item->appointment && (item->type == PST_TYPE_APPOINTMENT)) {
                    // Process Calendar Appointment item
                    DEBUG_MAIN(("main: Processing Appointment Entry\n"));
                    if (ff.type != PST_TYPE_APPOINTMENT) {
                        DEBUG_MAIN(("main: I have an appointment, but folder isn't specified as an appointment type. Processing...\n"));
                    }
                    printf("Appointment");
                    if (item->email && item->email->subject)
                        printf("\tSUMMARY: %s", pst_rfc2426_escape(item->email->subject->subj));
                    if (item->appointment->start)
                        printf("\tSTART: %s", pst_rfc2445_datetime_format(item->appointment->start));
                    if (item->appointment->end)
                        printf("\tEND: %s", pst_rfc2445_datetime_format(item->appointment->end));
                    printf("\tALL DAY: %s", (item->appointment->all_day==1 ? "Yes" : "No"));
                    printf("\n");

                } else {
                    ff.skip_count++;
                    DEBUG_MAIN(("main: Unknown item type. %i. Ascii1=\"%s\"\n",
                                      item->type, item->ascii_type));
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


int usage(char *prog_name) {
	DEBUG_ENT("usage");
	version();
	printf("Usage: %s [OPTIONS] {PST FILENAME}\n", prog_name);
	printf("OPTIONS:\n");
    printf("\t-d <filename> \t- Debug to file. This is a binary log. Use readlog to print it\n");
	printf("\t-h\t- Help. This screen\n");
	printf("\t-V\t- Version. Display program version\n");
	DEBUG_RET();
	return 0;
}


int version() {
	DEBUG_ENT("version");
	printf("lspst / LibPST v%s\n", VERSION);
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
	 return 0;
}


int main(int argc, char** argv) {
    pst_item *item = NULL;
    pst_desc_ll *d_ptr;
    char *temp  = NULL; //temporary char pointer
    int  c;
    char *d_log = NULL;

	while ((c = getopt(argc, argv, "d:hV"))!= -1) {
		switch (c) {
			case 'd':
				d_log = optarg;
				break;
			case 'h':
				usage(argv[0]);
				exit(0);
				break;
			case 'V':
				version();
				exit(0);
				break;
			default:
				usage(argv[0]);
				exit(1);
				break;
		}
	}

    #ifdef DEBUG_ALL
        // force a log file
        if (!d_log) d_log = "lspst.log";
    #endif // defined DEBUG_ALL
    DEBUG_INIT(d_log);
    DEBUG_REGISTER_CLOSE();
    DEBUG_ENT("main");

	if (argc <= optind) {
		usage(argv[0]);
		exit(2);
	}

    // Open PST file
    if (pst_open(&pstfile, argv[optind])) DIE(("Error opening File\n"));

    // Load PST index
    if (pst_load_index(&pstfile)) DIE(("Index Error\n"));

    pst_load_extended_attributes(&pstfile);

    d_ptr = pstfile.d_head; // first record is main record
    item  = pst_parse_item(&pstfile, d_ptr);
    if (!item || !item->message_store) {
        DEBUG_RET();
        DIE(("main: Could not get root record\n"));
    }

    // default the file_as to the same as the main filename if it doesn't exist
    if (!item->file_as) {
        if (!(temp = strrchr(argv[1], '/')))
            if (!(temp = strrchr(argv[1], '\\')))
                temp = argv[1];
            else
                temp++; // get past the "\\"
        else
            temp++; // get past the "/"
        item->file_as = (char*)xmalloc(strlen(temp)+1);
        strcpy(item->file_as, temp);
    }
    fprintf(stderr, "item->file_as = '%s'.\n", item->file_as);

    d_ptr = pst_getTopOfFolders(&pstfile, item);
    if (!d_ptr) DIE(("Top of folders record not found. Cannot continue\n"));
    DEBUG_MAIN(("d_ptr(TOF) = %p.\n", d_ptr));

    process(item, d_ptr->child);    // do the childred of TOPF
    pst_freeItem(item);
    pst_close(&pstfile);

    DEBUG_RET();
    return 0;
}


// This function will make sure that a filename is in cannonical form.  That
// is, it will replace any slashes, backslashes, or colons with underscores.
void canonicalize_filename(char *fname) {
    DEBUG_ENT("canonicalize_filename");
    if (fname == NULL) {
        DEBUG_RET();
        return;
    }
    while ((fname = strpbrk(fname, "/\\:")))
        *fname = '_';
    DEBUG_RET();
}


void debug_print(char *fmt, ...) {
    // shamlessly stolen from minprintf() in K&R pg. 156
    va_list ap;
    char *p, *sval;
    void *pval;
    int ival;
    double dval;
    FILE *fp = stderr;

    va_start(ap, fmt);
    for(p = fmt; *p; p++) {
        if (*p != '%') {
            fputc(*p, fp);
            continue;
        }
        switch (tolower(*++p)) {
            case 'd': case 'i':
                ival = va_arg(ap, int);
                fprintf(fp, "%d", ival);
                break;
            case 'f':
                dval = va_arg(ap, double);
                fprintf(fp, "%f", dval);
                break;
            case 's':
                for (sval = va_arg(ap, char *); *sval; ++sval)
                    fputc(*sval, fp);
                break;
            case 'p':
                pval = va_arg(ap, void *);
                fprintf(fp, "%p", pval);
                break;
            case 'x':
                ival = va_arg(ap, int);
                fprintf(fp, "%#010x", ival);
                break;
            default:
                fputc(*p, fp);
                break;
        }
    }
    va_end(ap);
}


