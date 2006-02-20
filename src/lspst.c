/***
 * lspst.c
 * Part of the LibPST project
 * Author: Joe Nahmias <joe@nahmias.net>
 * Based on readpst.c by by David Smith <dave.s@earthcorp.com>
 *
 */

// header file includes {{{1
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "libpst.h"
#include "define.h"
#include "timeconv.h"
// }}}1
// struct file_ll {{{1
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
// }}}1
// Function Declarations {{{1
void canonicalize_filename(char *fname);
int chr_count(char *str, char x);
void debug_print(char *fmt, ...);
char *rfc2426_escape(char *str);
char *rfc2445_datetime_format(FILETIME *ft);
// }}}1
#ifndef DEBUG_MAIN
	#define DEBUG_MAIN(x) debug_print x;
#endif
// int main(int argc, char** argv) {{{1
int main(int argc, char** argv) {

	// declarations {{{2
	pst_item *item = NULL;
	pst_file pstfile;
	pst_desc_ll *d_ptr;
	char *temp = NULL; //temporary char pointer
	int skip_child = 0;
	struct file_ll	*f, *head;
	// }}}2

	if (argc <= 1)
		DIE(("Missing PST filename.\n"));

	// Open PST file
	if ( pst_open(&pstfile, argv[1], "r") )
		 DIE(("Error opening File\n"));
	// Load PST index
	if ( pst_load_index(&pstfile) )
		DIE(("Index Error\n"));
	pst_load_extended_attributes(&pstfile);

	d_ptr = pstfile.d_head; // first record is main record
	if ((item = _pst_parse_item(&pstfile, d_ptr)) == NULL || item->message_store == NULL) {
		DIE(("main: Could not get root record\n"));
	}

	// default the file_as to the same as the main filename if it doesn't exist
	if (item->file_as == NULL) {
		if ((temp = strrchr(argv[1], '/')) == NULL)
			if ((temp = strrchr(argv[1], '\\')) == NULL)
				temp = argv[1];
			else
				temp++; // get past the "\\"
		else
			temp++; // get past the "/"
		item->file_as = (char*)xmalloc(strlen(temp)+1);
		strcpy(item->file_as, temp);
	}
	fprintf(stderr, "item->file_as = '%s'.\n", item->file_as);

	// setup head file_ll
	head = (struct file_ll*) malloc(sizeof(struct file_ll));
	memset(head, 0, sizeof(struct file_ll));
	head->email_count = 0;
	head->skip_count = 0;
	head->next = NULL;
	head->name = "mbox";
	head->dname = (char*) malloc(strlen(item->file_as)+1);
	strcpy(head->dname, item->file_as);
	head->type = item->type;
	DEBUG_MAIN(("head @ %p: name = '%s', dname = '%s', next = %p.\n", head, head->name, head->dname, head->next));

	if ((d_ptr = pst_getTopOfFolders(&pstfile, item)) == NULL) {
		DIE(("Top of folders record not found. Cannot continue\n"));
	}
	DEBUG_MAIN(("d_ptr(TOF) = %p.\n", d_ptr));

	if (item){
		_pst_freeItem(item);
		item = NULL;
	}

	d_ptr = d_ptr->child; // do the children of TOPF
	DEBUG_MAIN(("d_ptr(TOF->child) = %p.\n", d_ptr));

	DEBUG_MAIN(("main: About to do email stuff\n"));
	while (d_ptr != NULL) {
		// Process d_ptr {{{2
		DEBUG_MAIN(("main: New item record, d_ptr = %p.\n", d_ptr));
		if (d_ptr->desc == NULL) {
			DEBUG_WARN(("main: ERROR ?? item's desc record is NULL\n"));
			f->skip_count++;
			goto check_parent;
		}
		DEBUG_MAIN(("main: Desc Email ID %x [d_ptr->id = %x]\n", d_ptr->desc->id, d_ptr->id));

		item = _pst_parse_item(&pstfile, d_ptr);
		DEBUG_MAIN(("main: About to process item @ %p.\n", item));
		if (item != NULL) {

			// there should only be one message_store, and we have already
			// done it
			if (item->message_store != NULL) {
				DIE(("ERROR(main): A second message_store has been found.\n"));
			}

			if (item->folder != NULL) {
				// Process Folder item {{{3
				// if this is a folder, we want to recurse into it
				printf("Folder");
				if (item->file_as != NULL)
						printf("\t%s/", item->file_as);
				printf("\n");

				DEBUG_MAIN(("main: I think I may try to go into folder \"%s\"\n", item->file_as));
				f = (struct file_ll*) malloc(sizeof(struct file_ll));
				memset(f, 0, sizeof(struct file_ll));
				f->next = head;
				f->email_count = 0;
				f->type = item->type;
				f->stored_count = item->folder->email_count;
				head = f;
				f->name = "mbox";
				f->dname = (char*) xmalloc(strlen(item->file_as)+1);
				strcpy(f->dname, item->file_as);

				DEBUG_MAIN(("main: f->name = %s\nitem->folder_name = %s\n", f->name, item->file_as));
				canonicalize_filename(f->name);

				if (d_ptr->child != NULL) {
					d_ptr = d_ptr->child;
					skip_child = 1;
				} else {
					DEBUG_MAIN(("main: Folder has NO children. Creating directory, and closing again\n"));
					// printf("\tNo items to process in folder \"%s\", should have been %i\n", f->dname, f->stored_count);
					head = f->next;
					if (f->output != NULL)
						fclose(f->output);
					free(f->dname);
					free(f->name);
					free(f);

					f = head;
				}
				_pst_freeItem(item);
				item = NULL; // just for the odd situations!
				goto check_parent;
				// }}}3
			} else if (item->contact != NULL) {
				// Process Contact item {{{3
				if (f->type != PST_TYPE_CONTACT) {
					DEBUG_MAIN(("main: I have a contact, but the folder isn't a contacts folder. "
							 "Will process anyway\n"));
				}
				if (item->type != PST_TYPE_CONTACT) {
					DEBUG_MAIN(("main: I have an item that has contact info, but doesn't say that"
							 " it is a contact. Type is \"%s\"\n", item->ascii_type));
					DEBUG_MAIN(("main: Processing anyway\n"));
				}

				printf("Contact");
				if (item->contact->fullname != NULL)
					printf("\t%s", rfc2426_escape(item->contact->fullname));
				printf("\n");
				// }}}3
			} else if (item->email != NULL &&
					(item->type == PST_TYPE_NOTE || item->type == PST_TYPE_REPORT)) {
				// Process Email item {{{3
				printf("Email");
				if (item->email->outlook_sender_name != NULL)
					printf("\tFrom: %s", item->email->outlook_sender_name);
				if (item->email->subject->subj != NULL)
					printf("\tSubject: %s", item->email->subject->subj);
				printf("\n");
				// }}}3
			} else if (item->type == PST_TYPE_JOURNAL) {
				// Process Journal item {{{3
				if (f->type != PST_TYPE_JOURNAL) {
					DEBUG_MAIN(("main: I have a journal entry, but folder isn't specified as a journal type. Processing...\n"));
				}

				printf("Journal\t%s\n", rfc2426_escape(item->email->subject->subj));
				// }}}3
			} else if (item->type == PST_TYPE_APPOINTMENT) {
				// Process Calendar Appointment item {{{3
				// deal with Calendar appointments

				DEBUG_MAIN(("main: Processing Appointment Entry\n"));
				if (f->type != PST_TYPE_APPOINTMENT) {
					DEBUG_MAIN(("main: I have an appointment, but folder isn't specified as an appointment type. Processing...\n"));
				}

				printf("Appointment");
				if (item->email != NULL && item->email->subject != NULL)
					printf("\tSUMMARY: %s", rfc2426_escape(item->email->subject->subj));
				if (item->appointment != NULL && item->appointment->start != NULL)
					printf("\tSTART: %s", rfc2445_datetime_format(item->appointment->start));
				printf("\n");

				// }}}3
			} else {
				f->skip_count++;
				DEBUG_MAIN(("main: Unknown item type. %i. Ascii1=\"%s\"\n", \
					item->type, item->ascii_type));
			}
		} else {
			f->skip_count++;
			DEBUG_MAIN(("main: A NULL item was seen\n"));
		}

		check_parent:
		//	  _pst_freeItem(item);
		while (!skip_child && d_ptr->next == NULL && d_ptr->parent != NULL) {
			DEBUG_MAIN(("main: Going to Parent\n"));
			head = f->next;
			if (f->output != NULL)
				fclose(f->output);
			DEBUG_MAIN(("main: Email Count for folder %s is %i\n", f->dname, f->email_count));
			/*
			printf("\t\"%s\" - %i items done, skipped %i, should have been %i\n", \
					f->dname, f->email_count, f->skip_count, f->stored_count);
			*/

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

		if (d_ptr == NULL) { DEBUG_MAIN(("main: d_ptr is now NULL\n")); }

	// }}}2
	} // end while(d_ptr != NULL)
	DEBUG_MAIN(("main: Finished.\n"));

	// Cleanup {{{2
	pst_close(&pstfile);
	while (f != NULL) {
		if (f->output != NULL)
			fclose(f->output);
		free(f->name);
		free(f->dname);

		head = f->next;
		free(f);
		f = head;
	}
	DEBUG_RET();
	// }}}2

	return 0;
}
// }}}1
// void canonicalize_filename(char *fname) {{{1
// This function will make sure that a filename is in cannonical form.	That
// is, it will replace any slashes, backslashes, or colons with underscores.
void canonicalize_filename(char *fname) {
	DEBUG_ENT("canonicalize_filename");
	if (fname == NULL) {
		DEBUG_RET();
		return;
	}
	while ((fname = strpbrk(fname, "/\\:")) != NULL)
		*fname = '_';
	DEBUG_RET();
}
// }}}1
// int chr_count(char *str, char x) {{{1
int chr_count(char *str, char x) {
	int r = 0;
	if (str == NULL) return 0;
	while (*str != '\0') {
		if (*str == x)
			r++;
		str++;
	}
	return r;
}
// }}}1
// void debug_print(char *fmt, ...) {{{1
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
// }}}1
// char *rfc2426_escape(char *str) {{{1
char *rfc2426_escape(char *str) {
	static char *buf = NULL;
	char *a, *b;
	int y, z;

	DEBUG_ENT("rfc2426_escape");
	if (str == NULL) {
		DEBUG_RET();
		return NULL;
	}

	// calculate space required to escape all the commas, semi-colons, backslashes, and newlines
	y = chr_count(str, ',') + chr_count(str, '\\') + chr_count(str, ';') + chr_count(str, '\n');
	// count how many carriage-returns we have to skip
	z = chr_count(str, '\r');

	if (y == 0 && z == 0) {
		// there isn't any work required
		DEBUG_RET();
		return str;
	}

	buf = (char *) realloc( buf, strlen(str) + y - z + 1 );
	for (a = str, b = buf; *a != '\0'; ++a, ++b)
		switch (*a) {
			case ',' : case '\\': case ';' : case '\n':
				// insert backslash to escape
				*(b++) = '\\';
				*b = *a;
				break;
			case '\r':
				// skip
				break;
			default:
				*b = *a;
		}
	*b = '\0';  // NUL-terminate the string

	DEBUG_RET();
	return buf;
}
// }}}1
// char *rfc2445_datetime_format(FILETIME *ft) {{{1
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
// }}}1

// vim:sw=4 ts=4:
// vim600: set foldlevel=0 foldmethod=marker:
