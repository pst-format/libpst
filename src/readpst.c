/***
 * readpst.c
 * Part of the LibPST project
 * Written by David Smith
 *			  dave.s@earthcorp.com
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

// for reading of directory and clearing in function mk_seperate_dir
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

#include "version.h"
#include "define.h"
#include "libpst.h"
#include "common.h"
#include "timeconv.h"
#include "lzfu.h"
#define OUTPUT_TEMPLATE "%s"
#define OUTPUT_KMAIL_DIR_TEMPLATE ".%s.directory"
#define KMAIL_INDEX ".%s.index"
#define SEP_MAIL_FILE_TEMPLATE "%i" /* "%09i" */

// max size of the c_time char*. It will store the date of the email
#define C_TIME_SIZE 500
#define PERM_DIRS 0777

// macro used for creating directories
#ifndef WIN32
#define D_MKDIR(x) mkdir(x, PERM_DIRS)
#else
#define D_MKDIR(x) mkdir(x)
#endif
struct file_ll {
	char *name;
	char *dname;
	FILE * output;
	int32_t stored_count;
	int32_t email_count;
	int32_t skip_count;
	int32_t type;
	struct file_ll *next;
};
void  write_email_body(FILE *f, char *body);
char *removeCR (char *c);
int32_t   usage();
int32_t   version();
char *mk_kmail_dir(char*);
int32_t   close_kmail_dir();
char *mk_recurse_dir(char*);
int32_t   close_recurse_dir();
char *mk_seperate_dir(char *dir, int overwrite);
int32_t   close_seperate_dir();
int32_t   mk_seperate_file(struct file_ll *f);
char *my_stristr(char *haystack, char *needle);
char *check_filename(char *fname);
char *rfc2426_escape(char *str);
int32_t chr_count(char *str, char x);
char *rfc2425_datetime_format(FILETIME *ft);
char *rfc2445_datetime_format(FILETIME *ft);
char *skip_header_prologue(char *headers);
void write_separate_attachment(char f_name[], pst_item_attach* current_attach, int attach_num, pst_file* pst);
void write_inline_attachment(FILE* f_output, pst_item_attach* current_attach, char boundary[], pst_file* pst);
void write_normal_email(FILE* f_output, char f_name[], pst_item* item, int mode, int mode_MH, pst_file* pst);
void write_vcard(FILE* f_output, pst_item_contact* contact, char comment[]);
void write_appointment(FILE* f_output, pst_item_appointment* appointment,
		       pst_item_email* email, FILETIME* create_date, FILETIME* modify_date);
void create_enter_dir(struct file_ll* f, char file_as[], int mode, int overwrite);
char *prog_name;
char *output_dir = ".";
char *kmail_chdir = NULL;
// Normal mode just creates mbox format files in the current directory. Each file is named
// the same as the folder's name that it represents
#define MODE_NORMAL 0
// KMail mode creates a directory structure suitable for being used directly
// by the KMail application
#define MODE_KMAIL 1
// recurse mode creates a directory structure like the PST file. Each directory
// contains only one file which stores the emails in mbox format.
#define MODE_RECURSE 2
// seperate mode is similar directory structure to RECURSE. The emails are stored in
// seperate files, numbering from 1 upward. Attachments belonging to the emails are
// saved as email_no-filename (e.g. 1-samplefile.doc or 000001-Attachment2.zip)
#define MODE_SEPERATE 3


// Output Normal just prints the standard information about what is going on
#define OUTPUT_NORMAL 0
// Output Quiet is provided so that only errors are printed
#define OUTPUT_QUIET 1

// default mime-type for attachments that have a null mime-type
#define MIME_TYPE_DEFAULT "application/octet-stream"


// output mode for contacts
#define CMODE_VCARD 0
#define CMODE_LIST	1

// output settings for RTF bodies
// filename for the attachment
#define RTF_ATTACH_NAME "rtf-body.rtf"
// mime type for the attachment
#define RTF_ATTACH_TYPE "application/rtf"
int main(int argc, char** argv) {
	pst_item *item = NULL;
	pst_file pstfile;
	pst_desc_ll *d_ptr;
	char * fname = NULL;
	char *d_log=NULL;
	int c,x;
	int mode = MODE_NORMAL;
	int mode_MH = 0;
	int output_mode = OUTPUT_NORMAL;
	int contact_mode = CMODE_VCARD;
	int overwrite = 0;
	//  int encrypt = 0;
	char *temp = NULL; //temporary char pointer
	int skip_child = 0;
	struct file_ll  *f, *head;
	prog_name = argv[0];

	while ((c = getopt(argc, argv, "d:hko:qrMSVwc:"))!= -1) {
		switch (c) {
		case 'c':
			if (optarg!=NULL && optarg[0]=='v')
				contact_mode=CMODE_VCARD;
			else if (optarg!=NULL && optarg[0]=='l')
				contact_mode=CMODE_LIST;
			else {
				usage();
				exit(0);
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
		case 'k':
			mode = MODE_KMAIL;
			break;
		case 'M':
			mode = MODE_SEPERATE;
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
			mode = MODE_SEPERATE;
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

#ifdef DEBUG_ALL
	// initialize log file
	if (d_log == NULL)
		d_log = "readpst.log";
	DEBUG_INIT(d_log);
	DEBUG_REGISTER_CLOSE();
#endif // defined DEBUG_ALL

	DEBUG_ENT("main");

	if (argc > optind) {
		fname = argv[optind];
	} else {
		usage();
		exit(2);
	}

	if (output_mode != OUTPUT_QUIET) printf("Opening PST file and indexes...\n");

	DEBUG_MAIN(("main: Opening PST file '%s'\n", fname));
	RET_DERROR(pst_open(&pstfile, fname, "r"), 1, ("Error opening File\n"));
	DEBUG_MAIN(("main: Loading Indexes\n"));
	RET_DERROR(pst_load_index(&pstfile), 2, ("Index Error\n"));
	DEBUG_MAIN(("processing file items\n"));

	pst_load_extended_attributes(&pstfile);

	if (chdir(output_dir)) {
		x = errno;
		pst_close(&pstfile);
		DIE(("main: Cannot change to output dir %s: %s\n", output_dir, strerror(x)));
	}

	if (output_mode != OUTPUT_QUIET) printf("About to start processing first record...\n");

	d_ptr = pstfile.d_head; // first record is main record
	if ((item = _pst_parse_item(&pstfile, d_ptr)) == NULL || item->message_store == NULL) {
		DIE(("main: Could not get root record\n"));
	}

	// default the file_as to the same as the main filename if it doesn't exist
	if (item->file_as == NULL) {
		if ((temp = strrchr(fname, '/')) == NULL)
			if ((temp = strrchr(fname, '\\')) == NULL)
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


	f = (struct file_ll*) malloc(sizeof(struct file_ll));
	memset(f, 0, sizeof(struct file_ll));
	f->email_count = 0;
	f->skip_count = 0;
	f->next = NULL;
	head = f;
	create_enter_dir(f, item->file_as, mode, overwrite);
	f->type = item->type;

	if ((d_ptr = pst_getTopOfFolders(&pstfile, item)) == NULL) {
		DIE(("Top of folders record not found. Cannot continue\n"));
	}

	if (item){
		_pst_freeItem(item);
		item = NULL;
	}

	/*  if ((item = _pst_parse_item(&pstfile, d_ptr)) == NULL || item->folder == NULL) {
	    DEBUG_MAIN(("main: Could not get \"Top Of Personal Folder\" record\n"));
	    return -2;
	    }*/
	d_ptr = d_ptr->child; // do the children of TOPF

	if (output_mode != OUTPUT_QUIET) printf("Processing items...\n");

	DEBUG_MAIN(("main: About to do email stuff\n"));
	while (d_ptr != NULL) {
		DEBUG_MAIN(("main: New item record\n"));
		if (d_ptr->desc == NULL) {
			DEBUG_WARN(("main: ERROR ?? item's desc record is NULL\n"));
			f->skip_count++;
			goto check_parent;
		}
		DEBUG_MAIN(("main: Desc Email ID %#x [d_ptr->id = %#x]\n", d_ptr->desc->id, d_ptr->id));

		item = _pst_parse_item(&pstfile, d_ptr);
		DEBUG_MAIN(("main: About to process item\n"));
		if (item != NULL && item->email != NULL && item->email->subject != NULL &&
		    item->email->subject->subj != NULL) {
			//	  DEBUG_EMAIL(("item->email->subject = %p\n", item->email->subject));
			//	  DEBUG_EMAIL(("item->email->subject->subj = %p\n", item->email->subject->subj));
		}
		if (item != NULL) {
			if (item->message_store != NULL) {
				// there should only be one message_store, and we have already done it
				DIE(("main: A second message_store has been found. Sorry, this must be an error.\n"));
			}


			if (item->folder != NULL) {
				// if this is a folder, we want to recurse into it
				if (output_mode != OUTPUT_QUIET) printf("Processing Folder \"%s\"\n", item->file_as);
				//	f->email_count++;
				DEBUG_MAIN(("main: I think I may try to go into folder \"%s\"\n", item->file_as));
				f = (struct file_ll*) malloc(sizeof(struct file_ll));
				memset(f, 0, sizeof(struct file_ll));

				f->next = head;
				f->email_count = 0;
				f->type = item->type;
				f->stored_count = item->folder->email_count;
				head = f;

				temp = item->file_as;
				temp = check_filename(temp);
				create_enter_dir(f, item->file_as, mode, overwrite);
				if (d_ptr->child != NULL) {
					d_ptr = d_ptr->child;
					skip_child = 1;
				} else {
					DEBUG_MAIN(("main: Folder has NO children. Creating directory, and closing again\n"));
					if (output_mode != OUTPUT_QUIET)
						printf("\tNo items to process in folder \"%s\", should have been %i\n", f->dname, f->stored_count);
					head = f->next;
					if (f->output != NULL)
						fclose(f->output);
					if (mode == MODE_KMAIL)
						close_kmail_dir();
					else if (mode == MODE_RECURSE)
						close_recurse_dir();
					else if (mode == MODE_SEPERATE)
						close_seperate_dir();
					free(f->dname);
					free(f->name);
					free(f);

					f = head;
				}
				_pst_freeItem(item);
				item = NULL; // just for the odd situations!
				goto check_parent;
			} else if (item->contact != NULL) {
				// deal with a contact
				// write them to the file, one per line in this format
				// Desc Name <email@address>\n
				if (mode == MODE_SEPERATE) {
					mk_seperate_file(f);
				}
				f->email_count++;

				DEBUG_MAIN(("main: Processing Contact\n"));
				if (f->type != PST_TYPE_CONTACT) {
					DEBUG_MAIN(("main: I have a contact, but the folder isn't a contacts folder. "
						    "Will process anyway\n"));
				}
				if (item->type != PST_TYPE_CONTACT) {
					DEBUG_MAIN(("main: I have an item that has contact info, but doesn't say that"
						    " it is a contact. Type is \"%s\"\n", item->ascii_type));
					DEBUG_MAIN(("main: Processing anyway\n"));
				}
				if (item->contact == NULL) { // this is an incorrect situation. Inform user
					DEBUG_MAIN(("main: ERROR. This contact has not been fully parsed. one of the pre-requisties is NULL\n"));
				} else {
					if (contact_mode == CMODE_VCARD)
						write_vcard(f->output, item->contact, item->comment);
					else
						fprintf(f->output, "%s <%s>\n", item->contact->fullname, item->contact->address1);
				}
			} else if (item->email != NULL &&
				   (item->type == PST_TYPE_NOTE || item->type == PST_TYPE_REPORT)) {
				if (mode == MODE_SEPERATE) {
					mk_seperate_file(f);
				}

				f->email_count++;

				DEBUG_MAIN(("main: seen an email\n"));
				write_normal_email(f->output, f->name, item, mode, mode_MH, &pstfile);
			} else if (item->type == PST_TYPE_JOURNAL) {
				// deal with journal items
				if (mode == MODE_SEPERATE) {
					mk_seperate_file(f);
				}
				f->email_count++;

				DEBUG_MAIN(("main: Processing Journal Entry\n"));
				if (f->type != PST_TYPE_JOURNAL) {
					DEBUG_MAIN(("main: I have a journal entry, but folder isn't specified as a journal type. Processing...\n"));
				}

				/*	if (item->type != PST_TYPE_JOURNAL) {
					DEBUG_MAIN(("main: I have an item with journal info, but it's type is \"%s\" \n. Processing...\n",
					item->ascii_type));
					}*/
				fprintf(f->output, "BEGIN:VJOURNAL\n");
				if (item->email->subject != NULL)
					fprintf(f->output, "SUMMARY:%s\n", rfc2426_escape(item->email->subject->subj));
				if (item->email->body != NULL)
					fprintf(f->output, "DESCRIPTION:%s\n", rfc2426_escape(item->email->body));
				if (item->journal->start != NULL)
					fprintf(f->output, "DTSTART;VALUE=DATE-TIME:%s\n", rfc2445_datetime_format(item->journal->start));
				fprintf(f->output, "END:VJOURNAL\n\n");
			} else if (item->type == PST_TYPE_APPOINTMENT) {
				// deal with Calendar appointments
				if (mode == MODE_SEPERATE) {
					mk_seperate_file(f);
				}
				f->email_count++;

				DEBUG_MAIN(("main: Processing Appointment Entry\n"));
				if (f->type != PST_TYPE_APPOINTMENT) {
					DEBUG_MAIN(("main: I have an appointment, but folder isn't specified as an appointment type. Processing...\n"));
				}
				write_appointment(f->output, item->appointment, item->email, item->create_date, item->modify_date);
			} else {
				f->skip_count++;
				DEBUG_MAIN(("main: Unknown item type. %i. Ascii1=\"%s\"\n",
					    item->type, item->ascii_type));
			}
		} else {
			f->skip_count++;
			DEBUG_MAIN(("main: A NULL item was seen\n"));
		}

		DEBUG_MAIN(("main: Going to next d_ptr\n"));

	check_parent:
		//	  _pst_freeItem(item);
		while (!skip_child && d_ptr->next == NULL && d_ptr->parent != NULL) {
			DEBUG_MAIN(("main: Going to Parent\n"));
			head = f->next;
			if (f->output != NULL)
				fclose(f->output);
			DEBUG_MAIN(("main: Email Count for folder %s is %i\n", f->dname, f->email_count));
			if (output_mode != OUTPUT_QUIET)
				printf("\t\"%s\" - %i items done, skipped %i, should have been %i\n",
				       f->dname, f->email_count, f->skip_count, f->stored_count);
			if (mode == MODE_KMAIL)
				close_kmail_dir();
			else if (mode == MODE_RECURSE)
				close_recurse_dir();
			else if (mode == MODE_SEPERATE)
				close_seperate_dir();
			free(f->name);
			free(f->dname);
			free(f);
			f = head;
			if (head == NULL) { //we can't go higher. Must be at start?
				DEBUG_MAIN(("main: We are now trying to go above the highest level. We must be finished\n"));
				break; //from main while loop
			}
			d_ptr = d_ptr->parent;
			skip_child = 0;
		}

		if (item != NULL) {
			DEBUG_MAIN(("main: Freeing memory used by item\n"));
			_pst_freeItem(item);
			item = NULL;
		}

		if (!skip_child)
			d_ptr = d_ptr->next;
		else
			skip_child = 0;

		if (d_ptr == NULL) {
			DEBUG_MAIN(("main: d_ptr is now NULL\n"));
		}
	}
	if (output_mode != OUTPUT_QUIET) printf("Finished.\n");
	DEBUG_MAIN(("main: Finished.\n"));

	pst_close(&pstfile);
	//  fclose(pstfile.fp);
	while (f != NULL) {
		if (f->output != NULL)
			fclose(f->output);
		free(f->name);
		free(f->dname);

		if (mode == MODE_KMAIL)
			close_kmail_dir();
		else if (mode == MODE_RECURSE)
			close_recurse_dir();
		else if (mode == MODE_SEPERATE)
			// DO SOMETHING HERE
			;
		head = f->next;
		free (f);
		f = head;
	}

	DEBUG_RET();

	return 0;
}
void write_email_body(FILE *f, char *body) {
	char *n = body;
	//  DEBUG_MAIN(("write_email_body(): \"%s\"\n", body));
	DEBUG_ENT("write_email_body");
	while (n != NULL) {
		if (strncmp(body, "From ", 5) == 0)
			fprintf(f, ">");
		if ((n = strchr(body, '\n'))) {
			n++;
			fwrite(body, n-body, 1, f); //write just a line

			body = n;
		}
	}
	fwrite(body, strlen(body), 1, f);
	DEBUG_RET();
}
char *removeCR (char *c) {
	// converts /r/n to /n
	char *a, *b;
	DEBUG_ENT("removeCR");
	a = b = c;
	while (*a != '\0') {
		*b = *a;
		if (*a != '\r')
			b++;
		a++;
	}
	*b = '\0';
	DEBUG_RET();
	return c;
}
int usage() {
	DEBUG_ENT("usage");
	version();
	printf("Usage: %s [OPTIONS] {PST FILENAME}\n", prog_name);
	printf("OPTIONS:\n");
	printf("\t-c[v|l]\t- Set the Contact output mode. -cv = VCard, -cl = EMail list\n");
	printf("\t-d\t- Debug to file. This is a binary log. Use readlog to print it\n");
	printf("\t-h\t- Help. This screen\n");
	printf("\t-k\t- KMail. Output in kmail format\n");
	printf("\t-M\t- MH. Write emails in the MH format\n");
	printf("\t-o\t- Output Dir. Directory to write files to. CWD is changed *after* opening pst file\n");
	printf("\t-q\t- Quiet. Only print error messages\n");
	printf("\t-r\t- Recursive. Output in a recursive format\n");
	printf("\t-S\t- Seperate. Write emails in the seperate format\n");
	printf("\t-V\t- Version. Display program version\n");
	printf("\t-w\t- Overwrite any output mbox files\n");
	DEBUG_RET();
	return 0;
}
int version() {
	DEBUG_ENT("version");
	printf("ReadPST v%s\n", VERSION);
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
char *mk_kmail_dir(char *fname) {
	//change to that directory
	//make a directory based on OUTPUT_KMAIL_DIR_TEMPLATE
	//allocate space for OUTPUT_TEMPLATE and form a char* with fname
	//return that value
	char *dir, *out_name, *index;
	int x;
	DEBUG_ENT("mk_kmail_dir");
	if (kmail_chdir != NULL && chdir(kmail_chdir)) {
		x = errno;
		DIE(("mk_kmail_dir: Cannot change to directory %s: %s\n", kmail_chdir, strerror(x)));
	}
	dir = malloc(strlen(fname)+strlen(OUTPUT_KMAIL_DIR_TEMPLATE)+1);
	sprintf(dir, OUTPUT_KMAIL_DIR_TEMPLATE, fname);
	dir = check_filename(dir);
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
	if (kmail_chdir != NULL) { //only free kmail_chdir if not NULL. do not change directory
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
	dir = check_filename(dir);
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
char *mk_seperate_dir(char *dir, int overwrite) {
#if !defined(WIN32) && !defined(__CYGWIN__)
	DIR * sdir = NULL;
	struct dirent *dirent = NULL;
	struct stat *filestat = xmalloc(sizeof(struct stat));
#endif

	char *dir_name = NULL;
	int x = 0, y = 0;
	DEBUG_ENT("mk_seperate_dir");
	/*#if defined(WIN32) || defined(__CYGWIN__)
	  DIE(("mk_seperate_dir: Win32 applications cannot use this function yet.\n"));
	  #endif*/

	dir_name = xmalloc(strlen(dir)+10);

	do {
		if (y == 0)
			sprintf(dir_name, "%s", dir);
		else
			sprintf(dir_name, "%s" SEP_MAIL_FILE_TEMPLATE, dir, y); // enough for 9 digits allocated above

		dir_name = check_filename(dir_name);
		DEBUG_MAIN(("mk_seperate_dir: about to try creating %s\n", dir_name));
		if (D_MKDIR(dir_name)) {
			if (errno != EEXIST) { // if there is an error, and it doesn't already exist
				x = errno;
				DIE(("mk_seperate_dir: Cannot create directory %s: %s\n", dir, strerror(x)));
			}
		} else {
			break;
		}
		y++;
	} while (overwrite == 0);

	if (chdir (dir_name)) {
		x = errno;
		DIE(("mk_recurse_dir: Cannot change to directory %s: %s\n", dir, strerror(x)));
	}

	if (overwrite) {
		// we should probably delete all files from this directory
#if !defined(WIN32) && !defined(__CYGWIN__)
		if ((sdir = opendir("./")) == NULL) {
			WARN(("mk_seperate_dir: Cannot open dir \"%s\" for deletion of old contents\n", "./"));
		} else {
			while ((dirent = readdir(sdir)) != NULL) {
				if (lstat(dirent->d_name, filestat) != -1)
					if (S_ISREG(filestat->st_mode)) {
						if (unlink(dirent->d_name)) {
							y = errno;
							DIE(("mk_seperate_dir: unlink returned error on file %s: %s\n", dirent->d_name, strerror(y)));
						}
					}
			}
		}
#endif
	}

	// overwrite will never change during this function, it is just there so that
	//  if overwrite is set, we only go through this loop once.

	// we don't return a filename here cause it isn't necessary.
	DEBUG_RET();
	return NULL;
}
int close_seperate_dir() {
	int x;
	DEBUG_ENT("close_seperate_dir");
	if (chdir("..")) {
		x = errno;
		DIE(("close_seperate_dir: Cannot go up dir (..): %s\n", strerror(x)));
	}
	DEBUG_RET();
	return 0;
}
int mk_seperate_file(struct file_ll *f) {
	const int name_offset = 1;
	DEBUG_ENT("mk_seperate_file");
	DEBUG_MAIN(("mk_seperate_file: opening next file to save email\n"));
	if (f->email_count > 999999999) { // bigger than nine 9's
		DIE(("mk_seperate_file: The number of emails in this folder has become too high to handle"));
	}
	sprintf(f->name, SEP_MAIL_FILE_TEMPLATE, f->email_count + name_offset);
	if (f->output != NULL)
		fclose(f->output);
	f->output = NULL;
	f->name = check_filename(f->name);
	if ((f->output = fopen(f->name, "w")) == NULL) {
		DIE(("mk_seperate_file: Cannot open file to save email \"%s\"\n", f->name));
	}
	DEBUG_RET();
	return 0;
}
char *my_stristr(char *haystack, char *needle) {
// my_stristr varies from strstr in that its searches are case-insensitive
	char *x=haystack, *y=needle, *z = NULL;
	DEBUG_ENT("my_stristr");
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
	DEBUG_RET();
	return z;
}
char *check_filename(char *fname) {
	char *t = fname;
	DEBUG_ENT("check_filename");
	if (t == NULL) {
		DEBUG_RET();
		return fname;
	}
	while ((t = strpbrk(t, "/\\:")) != NULL) {
		// while there are characters in the second string that we don't want
		*t = '_'; //replace them with an underscore
	}
	DEBUG_RET();
	return fname;
}
char *rfc2426_escape(char *str) {
	static char* buf = NULL;
	char *ret, *a, *b;
	int x = 0, y, z;
	DEBUG_ENT("rfc2426_escape");
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
				case ',' :
				case '\\':
				case ';' :
				case '\n':
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
	DEBUG_RET();
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
char *rfc2425_datetime_format(FILETIME *ft) {
	static char * buffer = NULL;
	struct tm *stm = NULL;
	DEBUG_ENT("rfc2425_datetime_format");
	if (buffer == NULL)
		buffer = malloc(30); // should be enough for the date as defined below

	stm = fileTimeToStructTM(ft);
	//Year[4]-Month[2]-Day[2] Hour[2]:Min[2]:Sec[2]
	if (strftime(buffer, 30, "%Y-%m-%dT%H:%M:%SZ", stm)==0) {
		DEBUG_INFO(("Problem occured formatting date\n"));
	}
	DEBUG_RET();
	return buffer;
}
char *rfc2445_datetime_format(FILETIME *ft) {
	static char* buffer = NULL;
	struct tm *stm = NULL;
	DEBUG_ENT("rfc2445_datetime_format");
	if (buffer == NULL)
		buffer = malloc(30); // should be enough
	stm = fileTimeToStructTM(ft);
	if (strftime(buffer, 30, "%Y%m%dT%H%M%SZ", stm)==0) {
		DEBUG_INFO(("Problem occured formatting date\n"));
	}
	DEBUG_RET();
	return buffer;
}
// The sole purpose of this function is to bypass the pseudo-header prologue
// that Microsoft Outlook inserts at the beginning of the internet email
// headers for emails stored in their "Personal Folders" files.
char *skip_header_prologue(char *headers) {
	const char *bad = "Microsoft Mail Internet Headers";

	if ( strncmp(headers, bad, strlen(bad)) == 0 ) {
		// Found the offensive header prologue
		char *pc;

		pc = strchr(headers, '\n');
		return pc + 1;
	}

	return headers;
}

// vim:sw=4 ts=4:
// vim600: set foldlevel=0 foldmethod=marker:
void write_separate_attachment(char f_name[], pst_item_attach* current_attach, int attach_num, pst_file* pst)
{
	FILE *fp = NULL;
	int x = 0;
	char *temp;

	check_filename(f_name);
	if (current_attach->filename2 == NULL) {
		temp = xmalloc(strlen(f_name)+15);
		sprintf(temp, "%s-attach%i", f_name, attach_num);
	} else {
		temp = xmalloc(strlen(f_name)+strlen(current_attach->filename2)+15);
		do {
			if (fp != NULL) fclose(fp);
			if (x == 0)
				sprintf(temp, "%s-%s", f_name, current_attach->filename2);
			else
				sprintf(temp, "%s-%s-%i", f_name, current_attach->filename2, x);
		} while ((fp = fopen(temp, "r"))!=NULL && ++x < 99999999);
		if (x > 99999999) {
			DIE(("error finding attachment name. exhausted possibilities to %s\n", temp));
		}
	}
	DEBUG_MAIN(("write_separate_attachment: Saving attachment to %s\n", temp));
	if ((fp = fopen(temp, "w")) == NULL) {
		WARN(("write_separate_attachment: Cannot open attachment save file \"%s\"\n", temp));
	} else {
		if (current_attach->data != NULL)
			fwrite(current_attach->data, 1, current_attach->size, fp);
		else {
			pst_attach_to_file(pst, current_attach, fp);
		}
		fclose(fp);
	}
}

void write_inline_attachment(FILE* f_output, pst_item_attach* current_attach, char boundary[], pst_file* pst)
{
	char *enc; // base64 encoded attachment
	DEBUG_MAIN(("write_inline_attachment: Attachment Size is %i\n", item->current_attach->size));
	DEBUG_MAIN(("write_inline_attachment: Attachment Pointer is %p\n", item->current_attach->data));
	if (current_attach->data != NULL) {
		if ((enc = base64_encode (current_attach->data, current_attach->size)) == NULL) {
			DEBUG_MAIN(("write_inline_attachment: ERROR base64_encode returned NULL. Must have failed\n"));
			return;
		}
	}
	if (boundary) {
		fprintf(f_output, "\n--%s\n", boundary);
		if (current_attach->mimetype == NULL) {
			fprintf(f_output, "Content-type: %s\n", MIME_TYPE_DEFAULT);
		} else {
			fprintf(f_output, "Content-type: %s\n", current_attach->mimetype);
		}
		fprintf(f_output, "Content-transfer-encoding: base64\n");
		if (current_attach->filename2 == NULL) {
			fprintf(f_output, "Content-Disposition: inline\n\n");
		} else {
			fprintf(f_output, "Content-Disposition: attachment; filename=\"%s\"\n\n",
				current_attach->filename2);
		}
	}
	if (current_attach->data != NULL) {
		fwrite(enc, 1, strlen(enc), f_output);
		DEBUG_MAIN(("Attachment Size after encoding is %i\n", strlen(enc)));
	} else {
		pst_attach_to_file_base64(pst, current_attach, f_output);
	}
	fprintf(f_output, "\n\n");
}

void write_normal_email(FILE* f_output, char f_name[], pst_item* item, int mode, int mode_MH, pst_file* pst)
{
	char *boundary = NULL; // the boundary marker between multipart sections
	char *temp = NULL;
	int attach_num, base64_body = 0;
	time_t em_time;
	char *c_time;

	// convert the sent date if it exists, or set it to a fixed date
	if (item->email->sent_date != NULL) {
		em_time = fileTimeToUnixTime(item->email->sent_date, 0);
		c_time = ctime(&em_time);
		if (c_time != NULL)
			c_time[strlen(c_time)-1] = '\0'; //remove end \n
		else
			c_time = "Fri Dec 28 12:06:21 2001";
	} else
		c_time= "Fri Dec 28 12:06:21 2001";

	// we will always look at the header to discover some stuff
	if (item->email->header != NULL ) {
		char *b1, *b2;
		// see if there is a boundary variable there
		// this search MUST be made case insensitive (DONE).
		// Also, some check to find out if we
		// are looking at the boundary associated with content-type, and that the content
		// type really is "multipart"

		removeCR(item->email->header);

		if ((b2 = my_stristr(item->email->header, "boundary=")) != NULL) {
			b2 += strlen("boundary="); // move boundary to first char of marker
	    
			if (*b2 == '"') {
				b2++;
				b1 = strchr(b2, '"'); // find terminating quote
			} else {
				b1 = b2;
				while (isgraph(*b1)) // find first char that isn't part of boundary
					b1++;
			}
	    
			boundary = malloc ((b1-b2)+1); //malloc that length
			memset (boundary, 0, (b1-b2)+1);  // blank it
			strncpy(boundary, b2, b1-b2); // copy boundary to another variable
			b1 = b2 = boundary;
			while (*b2 != '\0') { // remove any CRs and Tabs
				if (*b2 != '\n' && *b2 != '\r' && *b2 != '\t') {
					*b1 = *b2;
					b1++;
				}
				b2++;
			}
			*b1 = '\0';
	    
			DEBUG_MAIN(("write_normal_email: Found boundary of - %s\n", boundary));
		} else {
			DEBUG_MAIN(("write_normal_email: boundary not found in header\n"));
		}

		// also possible to set 7bit encoding detection here.
		if ((b2 = my_stristr(item->email->header, "Content-Transfer-Encoding:")) != NULL) {
			if ((b2 = strchr(b2, ':')) != NULL) {
				b2++; // skip to the : at the end of the string
		
				while (*b2 == ' ' || *b2 == '\t')
					b2++;
				if (pst_strincmp(b2, "base64", 6)==0) {
					DEBUG_MAIN(("body is base64 encoded\n"));
					base64_body = 1;
				}
			} else {
				DEBUG_WARN(("found a ':' during the my_stristr, but not after that..\n"));
			}
		}
	    
	}

	DEBUG_MAIN(("write_normal_email: About to print Header\n"));

	if (item != NULL && item->email != NULL && item->email->subject != NULL &&
	    item->email->subject->subj != NULL) {
		DEBUG_EMAIL(("item->email->subject->subj = %p\n", item->email->subject->subj));
	}

	if (item->email->header != NULL) {
		// some of the headers we get from the file are not properly defined.
		// they can contain some email stuff too. We will cut off the header
		// when we see a \n\n or \r\n\r\n
		temp = strstr(item->email->header, "\n\n");

		if (temp != NULL) {
			DEBUG_MAIN(("write_normal_email: Found body text in header\n"));
			*temp = '\0';
		} else {
			temp = item->email->header + strlen(item->email->header) - 1;
			if(*temp == '\n')
				*temp = '\0';
		}
	  
		if (mode != MODE_SEPERATE) {
			char *soh = NULL;  // real start of headers.
			// don't put rubbish in if we are doing seperate
			fprintf(f_output, "From \"%s\" %s\n", item->email->outlook_sender_name, c_time);
			soh = skip_header_prologue(item->email->header);
			fprintf(f_output, "%s\n", soh);
		} else {
			fprintf(f_output, "%s\n", item->email->header);
		}
	} else {
		//make up our own header!
		if (mode != MODE_SEPERATE) {
			// don't want this first line for this mode
			if (item->email->outlook_sender_name != NULL) {
				temp = item->email->outlook_sender_name;
			} else {
				temp = "(readpst_null)";
			}
			fprintf(f_output, "From \"%s\" %s\n", temp, c_time);
		}
		if ((temp = item->email->outlook_sender) == NULL)
			temp = "";
		fprintf(f_output, "From: \"%s\" <%s>\n", item->email->outlook_sender_name, temp);
		if (item->email->subject != NULL) {
			fprintf(f_output, "Subject: %s\n", item->email->subject->subj);
		} else {
			fprintf(f_output, "Subject: \n");
		}
		fprintf(f_output, "To: %s\n", item->email->sentto_address);
		if (item->email->cc_address != NULL) {
			fprintf(f_output, "Cc: %s\n", item->email->cc_address);
		}
		if (item->email->sent_date != NULL) {
			c_time = (char*) xmalloc(C_TIME_SIZE);
			strftime(c_time, C_TIME_SIZE, "%a, %d %b %Y %H:%M:%S %z", gmtime(&em_time));
			fprintf(f_output, "Date: %s\n", c_time);
			free(c_time);
		}

		fprintf(f_output, "MIME-Version: 1.0\n");
	}

	if (boundary == NULL && (item->attach ||(item->email->body && item->email->htmlbody)
				 || item->email->rtf_compressed || item->email->encrypted_body
				 || item->email->encrypted_htmlbody)) {
		// we need to create a boundary here.
		DEBUG_EMAIL(("write_normal_email: must create own boundary. oh dear.\n"));
		boundary = malloc(50 * sizeof(char)); // allow 50 chars for boundary
		boundary[0] = '\0';
		sprintf(boundary, "--boundary-LibPST-iamunique-%i_-_-", rand());
		DEBUG_EMAIL(("write_normal_email: created boundary is %s\n", boundary));

		/* If boundary != NULL, then it has already been printed with existing
		 * headers.  Otherwise we generate it here and print it.
		 */
		if (item->attach != NULL) {
			// write the boundary stuff if we have attachments
			fprintf(f_output, "Content-type: multipart/mixed;\n\tboundary=\"%s\"\n",
				boundary);
		} else if (boundary != NULL) {
			// else if we have multipart/alternative then tell it so
			fprintf(f_output, "Content-type: multipart/alternative;\n\tboundary=\"%s\"\n",
				boundary);
		} else if (item->email->htmlbody) {
			fprintf(f_output, "Content-type: text/html\n");
		}
	}

	fprintf(f_output, "\n");

	DEBUG_MAIN(("write_normal_email: About to print Body\n"));

	if (item->email->body != NULL) {
		if (boundary) {
			fprintf(f_output, "\n--%s\n", boundary);
			fprintf(f_output, "Content-type: text/plain\n\n");
			if (base64_body)
				fprintf(f_output, "Content-Transfer-Encoding: base64\n");
		}
		removeCR(item->email->body);
		if (base64_body)
			write_email_body(f_output, base64_encode(item->email->body,
								 strlen(item->email->body)));
		else
			write_email_body(f_output, item->email->body);
	}
	
	if (item->email->htmlbody != NULL) {
		if (boundary) {
			fprintf(f_output, "\n--%s\n", boundary);
			fprintf(f_output, "Content-type: text/html\n\n");
			if (base64_body)
				fprintf(f_output, "Content-Transfer-Encoding: base64\n");
		}
		removeCR(item->email->htmlbody);
		if (base64_body)
			write_email_body(f_output, base64_encode(item->email->htmlbody,
								 strlen(item->email->htmlbody)));
		else
			write_email_body(f_output, item->email->htmlbody);
	}

	if (item->email->rtf_compressed != NULL) {
		DEBUG_MAIN(("Adding RTF body as attachment\n"));
		item->current_attach = (pst_item_attach*)xmalloc(sizeof(pst_item_attach));
		memset(item->current_attach, 0, sizeof(pst_item_attach));
		item->current_attach->next = item->attach;
		item->attach = item->current_attach;
		item->current_attach->data = lzfu_decompress(item->email->rtf_compressed);
		item->current_attach->filename2 = xmalloc(strlen(RTF_ATTACH_NAME)+2);
		strcpy(item->current_attach->filename2, RTF_ATTACH_NAME);
		item->current_attach->mimetype = xmalloc(strlen(RTF_ATTACH_TYPE)+2);
		strcpy(item->current_attach->mimetype, RTF_ATTACH_TYPE);
		memcpy(&(item->current_attach->size), item->email->rtf_compressed+sizeof(int32_t), sizeof(int32_t));
		LE32_CPU(item->current_attach->size);
		//	  item->email->rtf_compressed = ;
		//	  attach_num++;
	}
	if (item->email->encrypted_body || item->email->encrypted_htmlbody) {
		// if either the body or htmlbody is encrypted, add them as attachments
		if (item->email->encrypted_body) {
			DEBUG_MAIN(("Adding Encrypted Body as attachment\n"));
			item->current_attach = (pst_item_attach*) xmalloc(sizeof(pst_item_attach));
			memset(item->current_attach, 0, sizeof(pst_item_attach));
			item->current_attach->next = item->attach;
			item->attach = item->current_attach;
	    
			item->current_attach->data = item->email->encrypted_body;
			item->current_attach->size = item->email->encrypted_body_size;
			item->email->encrypted_body = NULL;
		}
		if (item->email->encrypted_htmlbody) {
			DEBUG_MAIN(("Adding encrypted HTML body as attachment\n"));
			item->current_attach = (pst_item_attach*) xmalloc(sizeof(pst_item_attach));
			memset(item->current_attach, 0, sizeof(pst_item_attach));
			item->current_attach->next = item->attach;
			item->attach = item->current_attach;

			item->current_attach->data = item->email->encrypted_htmlbody;
			item->current_attach->size = item->email->encrypted_htmlbody_size;
			item->email->encrypted_htmlbody = NULL;
		}
		write_email_body(f_output, "The body of this email is encrypted. This isn't supported yet, but the body is now an attachment\n");
	}
	// attachments
	attach_num = 0;
	for(item->current_attach = item->attach;
	    item->current_attach;
	    item->current_attach = item->current_attach->next) {
		DEBUG_MAIN(("write_normal_email: Attempting Attachment encoding\n"));
		if (item->current_attach->data == NULL) {
			DEBUG_MAIN(("write_normal_email: Data of attachment is NULL!. Size is supposed to be %i\n", item->current_attach->size));
		}
		attach_num++;
		if (mode == MODE_SEPERATE && !mode_MH)
			write_separate_attachment(f_name, item->current_attach, attach_num, pst);
		else
			write_inline_attachment(f_output, item->current_attach, boundary, pst);
	}
	if (mode != MODE_SEPERATE) { /* do not add a boundary after the last attachment for mode_MH */
		DEBUG_MAIN(("write_normal_email: Writing buffer between emails\n"));
		if (boundary)
			fprintf(f_output, "\n--%s--\n", boundary);
		fprintf(f_output, "\n\n");
	}
	if (boundary)
		free (boundary);
}

void write_vcard(FILE* f_output, pst_item_contact* contact, char comment[])
{
	// the specification I am following is (hopefully) RFC2426 vCard Mime Directory Profile
	fprintf(f_output, "BEGIN:VCARD\n");
	fprintf(f_output, "FN:%s\n", rfc2426_escape(contact->fullname));
	fprintf(f_output, "N:%s;%s;%s;%s;%s\n",
		rfc2426_escape((contact->surname==NULL?"":contact->surname)),
		rfc2426_escape((contact->first_name==NULL?"":contact->first_name)),
		rfc2426_escape((contact->middle_name==NULL?"":contact->middle_name)),
		rfc2426_escape((contact->display_name_prefix==NULL?"":contact->display_name_prefix)),
		rfc2426_escape((contact->suffix==NULL?"":contact->suffix)));
	if (contact->nickname != NULL)
		fprintf(f_output, "NICKNAME:%s\n", rfc2426_escape(contact->nickname));
	if (contact->address1 != NULL)
		fprintf(f_output, "EMAIL:%s\n", rfc2426_escape(contact->address1));
	if (contact->address2 != NULL)
		fprintf(f_output, "EMAIL:%s\n", rfc2426_escape(contact->address2));
	if (contact->address3 != NULL)
		fprintf(f_output, "EMAIL:%s\n", rfc2426_escape(contact->address3));
	if (contact->birthday != NULL)
		fprintf(f_output, "BDAY:%s\n", rfc2425_datetime_format(contact->birthday));
	if (contact->home_address != NULL) {
		fprintf(f_output, "ADR;TYPE=home:%s;%s;%s;%s;%s;%s;%s\n",
			rfc2426_escape((contact->home_po_box!=NULL?contact->home_po_box:"")),
			"", // extended Address
			rfc2426_escape((contact->home_street!=NULL?contact->home_street:"")),
			rfc2426_escape((contact->home_city!=NULL?contact->home_city:"")),
			rfc2426_escape((contact->home_state!=NULL?contact->home_state:"")),
			rfc2426_escape((contact->home_postal_code!=NULL?contact->home_postal_code:"")),
			rfc2426_escape((contact->home_country!=NULL?contact->home_country:"")));
		fprintf(f_output, "LABEL;TYPE=home:%s\n", rfc2426_escape(contact->home_address));
	}
	if (contact->business_address != NULL) {
		fprintf(f_output, "ADR;TYPE=work:%s;%s;%s;%s;%s;%s;%s\n",
			rfc2426_escape((contact->business_po_box!=NULL?contact->business_po_box:"")),
			"", // extended Address
			rfc2426_escape((contact->business_street!=NULL?contact->business_street:"")),
			rfc2426_escape((contact->business_city!=NULL?contact->business_city:"")),
			rfc2426_escape((contact->business_state!=NULL?contact->business_state:"")),
			rfc2426_escape((contact->business_postal_code!=NULL?contact->business_postal_code:"")),
			rfc2426_escape((contact->business_country!=NULL?contact->business_country:"")));
		fprintf(f_output, "LABEL;TYPE=work:%s\n", rfc2426_escape(contact->business_address));
	}
	if (contact->other_address != NULL) {
		fprintf(f_output, "ADR;TYPE=postal:%s;%s;%s;%s;%s;%s;%s\n",
			rfc2426_escape((contact->other_po_box != NULL ?
					contact->business_po_box:"")),
			"", // extended Address
			rfc2426_escape((contact->other_street != NULL ?
					contact->other_street:"")),
			rfc2426_escape((contact->other_city != NULL ?
					contact->other_city:"")),
			rfc2426_escape((contact->other_state != NULL ?
					contact->other_state:"")),
			rfc2426_escape((contact->other_postal_code != NULL ?
					contact->other_postal_code:"")),
			rfc2426_escape((contact->other_country != NULL ?
					contact->other_country:"")));
		fprintf(f_output, "ADR;TYPE=postal:%s\n",
			rfc2426_escape(contact->other_address));
	}
	if (contact->business_fax != NULL)
		fprintf(f_output, "TEL;TYPE=work,fax:%s\n",
			rfc2426_escape(contact->business_fax));
	if (contact->business_phone != NULL)
		fprintf(f_output, "TEL;TYPE=work,voice:%s\n",
			rfc2426_escape(contact->business_phone));
	if (contact->business_phone2 != NULL)
		fprintf(f_output, "TEL;TYPE=work,voice:%s\n",
			rfc2426_escape(contact->business_phone2));
	if (contact->car_phone != NULL)
		fprintf(f_output, "TEL;TYPE=car,voice:%s\n",
			rfc2426_escape(contact->car_phone));
	if (contact->home_fax != NULL)
		fprintf(f_output, "TEL;TYPE=home,fax:%s\n",
			rfc2426_escape(contact->home_fax));
	if (contact->home_phone != NULL)
		fprintf(f_output, "TEL;TYPE=home,voice:%s\n",
			rfc2426_escape(contact->home_phone));
	if (contact->home_phone2 != NULL)
		fprintf(f_output, "TEL;TYPE=home,voice:%s\n",
			rfc2426_escape(contact->home_phone2));
	if (contact->isdn_phone != NULL)
		fprintf(f_output, "TEL;TYPE=isdn:%s\n",
			rfc2426_escape(contact->isdn_phone));
	if (contact->mobile_phone != NULL)
		fprintf(f_output, "TEL;TYPE=cell,voice:%s\n",
			rfc2426_escape(contact->mobile_phone));
	if (contact->other_phone != NULL)
		fprintf(f_output, "TEL;TYPE=msg:%s\n",
			rfc2426_escape(contact->other_phone));
	if (contact->pager_phone != NULL)
		fprintf(f_output, "TEL;TYPE=pager:%s\n",
			rfc2426_escape(contact->pager_phone));
	if (contact->primary_fax != NULL)
		fprintf(f_output, "TEL;TYPE=fax,pref:%s\n",
			rfc2426_escape(contact->primary_fax));
	if (contact->primary_phone != NULL)
		fprintf(f_output, "TEL;TYPE=phone,pref:%s\n",
			rfc2426_escape(contact->primary_phone));
	if (contact->radio_phone != NULL)
		fprintf(f_output, "TEL;TYPE=pcs:%s\n",
			rfc2426_escape(contact->radio_phone));
	if (contact->telex != NULL)
		fprintf(f_output, "TEL;TYPE=bbs:%s\n",
			rfc2426_escape(contact->telex));
	if (contact->job_title != NULL)
		fprintf(f_output, "TITLE:%s\n",
			rfc2426_escape(contact->job_title));
	if (contact->profession != NULL)
		fprintf(f_output, "ROLE:%s\n",
			rfc2426_escape(contact->profession));
	if (contact->assistant_name != NULL
	    || contact->assistant_phone != NULL) {
		fprintf(f_output, "AGENT:BEGIN:VCARD\\n");
		if (contact->assistant_name != NULL)
			fprintf(f_output, "FN:%s\\n",
				rfc2426_escape(contact->assistant_name));
		if (contact->assistant_phone != NULL)
			fprintf(f_output, "TEL:%s\\n",
				rfc2426_escape(contact->assistant_phone));
	}
	if (contact->company_name != NULL)
		fprintf(f_output, "ORG:%s\n",
			rfc2426_escape(contact->company_name));
	if (comment != NULL)
		fprintf(f_output, "NOTE:%s\n", rfc2426_escape(comment));

	fprintf(f_output, "VERSION: 3.0\n");
	fprintf(f_output, "END:VCARD\n\n");
}

void write_appointment(FILE* f_output, pst_item_appointment* appointment,
		       pst_item_email* email, FILETIME* create_date, FILETIME* modify_date)
{
	fprintf(f_output, "BEGIN:VEVENT\n");
	if (create_date != NULL)
		fprintf(f_output, "CREATED:%s\n",
			rfc2445_datetime_format(create_date));
	if (modify_date != NULL)
		fprintf(f_output, "LAST-MOD:%s\n",
			rfc2445_datetime_format(modify_date));
	if (email != NULL && email->subject != NULL)
		fprintf(f_output, "SUMMARY:%s\n",
			rfc2426_escape(email->subject->subj));
	if (email != NULL && email->body != NULL)
		fprintf(f_output, "DESCRIPTION:%s\n",
			rfc2426_escape(email->body));
	if (appointment != NULL && appointment->start != NULL)
		fprintf(f_output, "DTSTART;VALUE=DATE-TIME:%s\n",
			rfc2445_datetime_format(appointment->start));
	if (appointment != NULL && appointment->end != NULL)
		fprintf(f_output, "DTEND;VALUE=DATE-TIME:%s\n",
			rfc2445_datetime_format(appointment->end));
	if (appointment != NULL && appointment->location != NULL)
		fprintf(f_output, "LOCATION:%s\n",
			rfc2426_escape(appointment->location));
	if (appointment != NULL) {
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

void create_enter_dir(struct file_ll* f, char file_as[], int mode, int overwrite)
{
	if (mode == MODE_KMAIL)
		f->name = mk_kmail_dir(file_as); //create directory and form filename
	else if (mode == MODE_RECURSE)
		f->name = mk_recurse_dir(file_as);
	else if (mode == MODE_SEPERATE) {
		// do similar stuff to recurse here.
		mk_seperate_dir(file_as, overwrite);
		f->name = (char*) xmalloc(10);
		memset(f->name, 0, 10);
		//		sprintf(f->name, SEP_MAIL_FILE_TEMPLATE, f->email_count);
	} else {
		f->name = (char*) xmalloc(strlen(file_as)+strlen(OUTPUT_TEMPLATE+1));
		sprintf(f->name, OUTPUT_TEMPLATE, file_as);
	}

	f->dname = (char*) xmalloc(strlen(file_as)+1);
	strcpy(f->dname, file_as);

	if (overwrite != 1) {
		int x = 0;
		char *temp = (char*) xmalloc (strlen(f->name)+10); //enough room for 10 digits

		sprintf(temp, "%s", f->name);
		temp = check_filename(temp);
		while ((f->output = fopen(temp, "r")) != NULL) {
			DEBUG_MAIN(("create_enter_dir: need to increase filename cause one already exists with that name\n"));
			DEBUG_MAIN(("create_enter_dir: - increasing it to %s%d\n", f->name, x));
			x++;
			sprintf(temp, "%s%08d", f->name, x);
			DEBUG_MAIN(("create_enter_dir: - trying \"%s\"\n", f->name));
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

	DEBUG_MAIN(("create_enter_dir: f->name = %s\nitem->folder_name = %s\n", f->name, file_as));
	if (mode != MODE_SEPERATE) {
		f->name = check_filename(f->name);
		if ((f->output = fopen(f->name, "w")) == NULL) {
			DIE(("create_enter_dir: Could not open file \"%s\" for write\n", f->name));
		}
	}
}

