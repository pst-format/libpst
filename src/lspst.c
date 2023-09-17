/***
 * lspst.c
 * Part of the LibPST project
 * Author: Joe Nahmias <joe@nahmias.net>
 * Based on readpst.c by David Smith <dave.s@earthcorp.com>
 *
 */

#include "define.h"

struct file_ll {
    char *dname;
    int32_t stored_count;
    int32_t item_count;
    int32_t skip_count;
    int32_t type;
};

struct options {
    int long_format;
    char *date_format;
};

void canonicalize_filename(char *fname);
void debug_print(char *fmt, ...);
void usage(char *prog_name, char *defaultfmtdate);
void version();

// global settings
pst_file pstfile;


void create_enter_dir(struct file_ll* f, pst_item *item)
{
    pst_convert_utf8(item, &item->file_as);
    f->item_count   = 0;
    f->skip_count   = 0;
    f->type         = item->type;
    f->stored_count = (item->folder) ? item->folder->item_count : 0;
    f->dname        = strdup(item->file_as.str);
}


void close_enter_dir(struct file_ll *f)
{
    free(f->dname);
}

void process(pst_item *outeritem, pst_desc_tree *d_ptr, struct options o)
{
    struct file_ll ff;
    pst_item *item = NULL;
    char *result = NULL;
    size_t resultlen = 0;
    size_t dateresultlen;

    DEBUG_ENT("process");
    memset(&ff, 0, sizeof(ff));
    create_enter_dir(&ff, outeritem);

    while (d_ptr) {
        if (!d_ptr->desc) {
            DEBUG_WARN(("ERROR item's desc record is NULL\n"));
            ff.skip_count++;
        }
        else {
            DEBUG_INFO(("Desc Email ID %" PRIx64 " [d_ptr->d_id = %" PRIx64 "]\n", d_ptr->desc->i_id, d_ptr->d_id));

            item = pst_parse_item(&pstfile, d_ptr, NULL);
            DEBUG_INFO(("About to process item @ %p.\n", (void*)item));
            if (item) {
                if (item->message_store) {
                    // there should only be one message_store, and we have already done it
                    DIE(("A second message_store has been found. Sorry, this must be an error.\n"));
                }

                if (item->folder && d_ptr->child) {
                    // if this is a folder, we want to recurse into it
                    pst_convert_utf8(item, &item->file_as);
                    printf("Folder \"%s\"\n", item->file_as.str);
                    process(item, d_ptr->child, o);

                } else if (item->contact && (item->type == PST_TYPE_CONTACT)) {
                    if (!ff.type) ff.type = item->type;
                    // Process Contact item
                    if (ff.type != PST_TYPE_CONTACT) {
                        DEBUG_INFO(("I have a contact, but the folder isn't a contacts folder. Processing anyway\n"));
                    }
                    printf("Contact");
                    if (item->contact->fullname.str)
                        printf("\t%s", pst_rfc2426_escape(item->contact->fullname.str, &result, &resultlen));
                    printf("\n");

                } else if (item->email && ((item->type == PST_TYPE_NOTE) || (item->type == PST_TYPE_SCHEDULE) || (item->type == PST_TYPE_REPORT))) {
                    if (!ff.type) ff.type = item->type;
                    // Process Email item
                    if ((ff.type != PST_TYPE_NOTE) && (ff.type != PST_TYPE_SCHEDULE) && (ff.type != PST_TYPE_REPORT)) {
                        DEBUG_INFO(("I have an email, but the folder isn't an email folder. Processing anyway\n"));
                    }
                    printf("Email");
                    if (o.long_format == 1) {
                        if (item->email->arrival_date) {
                            char time_buffer[MAXDATEFMTLEN];
                            dateresultlen = pst_fileTimeToString(item->email->arrival_date, o.date_format, time_buffer);
                            if (dateresultlen < 1)
                                DIE(("Date format error in -f option.\n"));
                            printf("\tDate: %s", time_buffer);
                        }
                        else
                            printf("\t");
                    }
                    if (item->email->outlook_sender_name.str)
                        printf("\tFrom: %s", item->email->outlook_sender_name.str);
                    else
                        printf("\t");
                    if (o.long_format == 1) {
                        if (item->email->outlook_recipient_name.str)
                            printf("\tTo: %s", item->email->outlook_recipient_name.str);
                        else
                            printf("\t");
                        if (item->email->cc_address.str)
                            printf("\tCC: %s", item->email->cc_address.str);
                        else
                            printf("\t");
                        if (item->email->bcc_address.str)
                            printf("\tBCC: %s", item->email->bcc_address.str);
                        else
                            printf("\t");
                    }
                    if (item->subject.str)
                        printf("\tSubject: %s", item->subject.str);
                    else
                        printf("\t");
                    printf("\n");

                } else if (item->journal && (item->type == PST_TYPE_JOURNAL)) {
                    if (!ff.type) ff.type = item->type;
                    // Process Journal item
                    if (ff.type != PST_TYPE_JOURNAL) {
                        DEBUG_INFO(("I have a journal entry, but folder isn't specified as a journal type. Processing...\n"));
                    }
                    if (item->subject.str)
                        printf("Journal\t%s\n", pst_rfc2426_escape(item->subject.str, &result, &resultlen));

                } else if (item->appointment && (item->type == PST_TYPE_APPOINTMENT)) {
                    char time_buffer[30];
                    if (!ff.type) ff.type = item->type;
                    // Process Calendar Appointment item
                    DEBUG_INFO(("Processing Appointment Entry\n"));
                    if (ff.type != PST_TYPE_APPOINTMENT) {
                        DEBUG_INFO(("I have an appointment, but folder isn't specified as an appointment type. Processing...\n"));
                    }
                    printf("Appointment");
                    if (item->subject.str)
                        printf("\tSUMMARY: %s", pst_rfc2426_escape(item->subject.str, &result, &resultlen));
                    if (item->appointment->start)
                        printf("\tSTART: %s", pst_rfc2445_datetime_format(item->appointment->start, sizeof(time_buffer), time_buffer));
                    if (item->appointment->end)
                        printf("\tEND: %s", pst_rfc2445_datetime_format(item->appointment->end, sizeof(time_buffer), time_buffer));
                    printf("\tALL DAY: %s", (item->appointment->all_day==1 ? "Yes" : "No"));
                    printf("\n");

                } else {
                    ff.skip_count++;
                    DEBUG_INFO(("Unknown item type. %i. Ascii1=\"%s\"\n",
                                      item->type, item->ascii_type));
                }
                pst_freeItem(item);
            } else {
                ff.skip_count++;
                DEBUG_INFO(("A NULL item was seen\n"));
            }
        }
        d_ptr = d_ptr->next;
    }
    close_enter_dir(&ff);
    if (result) free(result);
    DEBUG_RET();
}


void usage(char *prog_name, char *defaultfmtdate) {
    DEBUG_ENT("usage");
    version();
    printf("Usage: %s [OPTIONS] {PST FILENAME}\n", prog_name);
    printf("OPTIONS:\n");
    printf("\t-d <filename> \t- Debug to file.\n");
    printf("\t-l\t- Print the date, CC and BCC fields of emails too (by default only the From and Subject)\n");
    printf("\t-f <date_format> \t- Select the date format in ctime format (by default \"%s\")\n", defaultfmtdate);
    printf("\t-h\t- Help. This screen\n");
    printf("\t-V\t- Version. Display program version\n");
    DEBUG_RET();
}


void version() {
    DEBUG_ENT("version");
    printf("lspst / LibPST v%s\n", VERSION);
#if BYTE_ORDER == BIG_ENDIAN
    printf("Big Endian implementation being used.\n");
#elif BYTE_ORDER == LITTLE_ENDIAN
    printf("Little Endian implementation being used.\n");
#else
#   error "Byte order not supported by this library"
#endif
    DEBUG_RET();
}


int main(int argc, char* const* argv) {
    pst_item *item = NULL;
    pst_desc_tree *d_ptr;
    char *temp  = NULL; //temporary char pointer
    int  c;
    char *d_log = NULL;
    struct options o;
    o.long_format = 0;
    char *defaultfmtdate = "%F %T";
    o.date_format = defaultfmtdate;

    while ((c = getopt(argc, argv, "d:f:lhV"))!= -1) {
        switch (c) {
            case 'd':
                d_log = optarg;
            break;
            case 'f':
                o.date_format = optarg;
            break;
            case 'l':
                o.long_format = 1;
            break;
            case 'h':
                usage(argv[0], defaultfmtdate);
                exit(0);
            break;
            case 'V':
                version();
                exit(0);
            break;
            default:
                usage(argv[0], defaultfmtdate);
                exit(1);
            break;
        }
    }

    #ifdef DEBUG_ALL
        // force a log file
        if (!d_log) d_log = "lspst.log";
    #endif // defined DEBUG_ALL
    DEBUG_INIT(d_log, NULL);
    DEBUG_ENT("main");

    if (argc <= optind) {
        usage(argv[0], defaultfmtdate);
        DEBUG_CLOSE();
        exit(2);
    }

    // Open PST file
    if (pst_open(&pstfile, argv[optind], NULL)) DIE(("Error opening File\n"));

    // Load PST index
    if (pst_load_index(&pstfile)) {
        pst_close(&pstfile);
        DIE(("Index Error\n"));
    }

    pst_load_extended_attributes(&pstfile);

    d_ptr = pstfile.d_head; // first record is main record
    item  = pst_parse_item(&pstfile, d_ptr, NULL);
    if (!item || !item->message_store) {
        pst_close(&pstfile);
        DEBUG_RET();
        DIE(("Could not get root record\n"));
    }

    // default the file_as to the same as the main filename if it doesn't exist
    if (!item->file_as.str) {
        if (!(temp = strrchr(argv[1], '/')))
            if (!(temp = strrchr(argv[1], '\\')))
                temp = argv[1];
            else
                temp++; // get past the "\\"
        else
            temp++; // get past the "/"
        item->file_as.str = strdup(temp);
        item->file_as.is_utf8 = 1;
    }

    d_ptr = pst_getTopOfFolders(&pstfile, item);
    if (!d_ptr) {
        pst_close(&pstfile);
        DIE(("Top of folders record not found. Cannot continue\n"));
    }

    process(item, d_ptr->child, o);    // do the children of TOPF
    pst_freeItem(item);
    pst_close(&pstfile);

    DEBUG_RET();
    DEBUG_CLOSE();
    return 0;
}


// This function will make sure that a filename is in canonical form.  That
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


