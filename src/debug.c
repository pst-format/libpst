/* Contains the debug functions */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include "define.h"

#ifdef _WIN32
# define vsnprintf _vsnprintf
#endif

struct pst_debug_item {
    int type;
    char * function;
    unsigned int line;
    char * file;
    char * text;
    struct pst_debug_item *next;
} *item_head=NULL, *item_tail=NULL, *item_ptr=NULL, *info_ptr=NULL, *temp_list=NULL;


struct pst_debug_func {
    char * name;
    struct pst_debug_func *next;
} *func_head=NULL, *func_ptr=NULL;


void pst_debug_write_msg(struct pst_debug_item *item, char *fmt, va_list *ap, int size);
void pst_debug_write_hex(struct pst_debug_item *item, unsigned char *buf, size_t size, int col);
void * xmalloc(size_t size);

// the largest text size we will store in memory. Otherwise we
// will do a debug_write, then create a new record, and write the
// text body directly to the file
#define MAX_MESSAGE_SIZE 4096

void pst_debug(char *fmt, ...) {
    va_list ap;
    va_start(ap,fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}


#define NUM_COL 30
void pst_debug_hexdumper(FILE *out, unsigned char *buf, size_t size, int col, int delta) {
    int off = 0, toff;
    int count = 0;

    if (!out) return;   // no file
    if (col == -1) col = NUM_COL;
    fprintf(out, "\n");
    while (off < size) {
        fprintf(out, "%06X\t:", off+delta);
        toff = off;
        while (count < col && off < size) {
            fprintf(out, "%02hhx ", buf[off]);
            off++; count++;
        }
        off = toff;
        while (count < col) {
            // only happens at end of block to pad the text over to the text column
            fprintf(out, "   ");
            count++;
        }
        count = 0;
        fprintf(out, ":");
        while (count < col && off < size) {
            fprintf(out, "%c", isgraph(buf[off])?buf[off]:'.');
            off++; count ++;
        }

        fprintf(out, "\n");
        count=0;
    }

    fprintf(out, "\n");
}


FILE *debug_fp = NULL;
unsigned int max_items=DEBUG_MAX_ITEMS, curr_items=0;


void pst_debug_init(char* fname) {
    unsigned char version = DEBUG_VERSION;
    item_head = item_tail = NULL;
    curr_items = 0;
    if (debug_fp) pst_debug_close();
    if (!fname) return;
    if ((debug_fp = fopen(fname, "wb")) == NULL) {
      fprintf(stderr, "Opening of file %s failed\n", fname);
      exit(1);
    }
    fwrite(&version, 1, sizeof(char), debug_fp);
}


// function must be called before pst_debug_msg. It sets up the
// structure for the function that follows
void pst_debug_msg_info(int line, char* file, int type) {
    char *x;
    if (!debug_fp) return;  // no file
    info_ptr = (struct pst_debug_item*) xmalloc(sizeof(struct pst_debug_item));
    info_ptr->type = type;
    info_ptr->line = line;
    x = (func_head==NULL?"No Function":func_head->name);
    info_ptr->function = (char*) xmalloc(strlen(x)+1);
    strcpy(info_ptr->function, x);

    info_ptr->file = (char*) xmalloc(strlen(file)+1);
    strcpy(info_ptr->file, file);

    //put the current record on a temp linked list
    info_ptr->next = temp_list;
    temp_list = info_ptr;
}


void pst_debug_msg_text(char* fmt, ...) {
    va_list ap;
    int f, g;
    char x[2];
    struct pst_debug_item *temp;
    if (!debug_fp) return;  // no file
    va_start(ap, fmt);
    // get the record off of the temp_list
    info_ptr = temp_list;
    if (info_ptr)
        temp_list = info_ptr->next;
    else {
        fprintf(stderr, "NULL info_ptr. ERROR!!\n");
        exit(-2);
    }
    // according to glibc 2.1, this should return the req. number of bytes for
    // the string
  #ifdef _WIN32
    // vsnprintf trick doesn't work. must use function called _vscprintf
    // cannot find much documentation about this on internet or anywhere.
    // I assume it isn't a standard function, but only in VisualC++
    f = _vscprintf(fmt, ap);
  #else
    f = vsnprintf(x, 1, fmt, ap);
  #endif
    va_end(ap);  // must be called after vsnprintf()

    if (f > 0 && f < MAX_MESSAGE_SIZE) {
        info_ptr->text = (char*) xmalloc(f+1);
        va_start(ap, fmt);
        if ((g = vsnprintf(info_ptr->text, f, fmt, ap)) == -1) {
            fprintf(stderr, "_debug_msg: Dieing! vsnprintf returned -1 for format \"%s\"\n", fmt);
            exit(-2);
        }
        va_end(ap);
        info_ptr->text[g] = '\0';
        if (f != g) {
            fprintf(stderr, "_debug_msg: f != g\n");
        }
    } else if (f > 0) { // it is over the max_message_size then
        f += strlen(info_ptr->file)+strlen(info_ptr->function);
        temp = info_ptr;
        pst_debug_write(); // dump the current messages
        info_ptr = temp;
        va_start(ap, fmt);
        pst_debug_write_msg(info_ptr, fmt, &ap, f);
        va_end(ap);
        free(info_ptr->function);
        free(info_ptr->file);
        free(info_ptr);
        info_ptr = NULL;
        return;
    } else {
        fprintf(stderr, "_debug_msg: error getting requested size of debug message\n");
        info_ptr->text = "ERROR Saving\n";
    }

    if (!item_head)
      item_head = info_ptr;

    info_ptr->next = NULL;
    if (item_tail) item_tail->next = info_ptr;
    item_tail = info_ptr;

    if (++curr_items == max_items) {
        // here we will jump off and save the contents
        pst_debug_write();
        info_ptr = NULL;
    }
}


void pst_debug_hexdump(unsigned char *x, size_t y, int cols, int delta) {
    struct pst_debug_item *temp;
    if (!debug_fp) return;  // no file
    info_ptr = temp_list;
    if (info_ptr) temp_list = info_ptr->next;
    temp = info_ptr;
    pst_debug_write();
    info_ptr = temp;
    pst_debug_write_hex(info_ptr, x, y, cols);
    free(info_ptr->function);
    free(info_ptr->file);
    free(info_ptr);
    info_ptr = NULL;
}


void pst_debug_func(char *function) {
    func_ptr = xmalloc (sizeof(struct pst_debug_func));
    func_ptr->name = xmalloc(strlen(function)+1);
    strcpy(func_ptr->name, function);
    func_ptr->next = func_head;
    func_head = func_ptr;
}


void pst_debug_func_ret() {
    //remove the head item
    func_ptr = func_head;
    if (func_head) {
        func_head = func_head->next;
        free(func_ptr->name);
        free(func_ptr);
    } else {
        DIE(("function list is empty!\n"));
    }
}


void pst_debug_close(void) {
    pst_debug_write();
    while (func_head) {
        func_ptr = func_head;
        func_head = func_head->next;
        free(func_ptr->name);
        free(func_ptr);
    }
    if (debug_fp) fclose(debug_fp);
    debug_fp = NULL;
}


void pst_debug_write() {
    size_t size, ptr, funcname, filename, text, end;
    char *buf = NULL, rec_type;
    if (!debug_fp) return;  // no file
    off_t index_pos = ftell (debug_fp);
    off_t file_pos  = index_pos;
    // add 2. One for the pointer to the next index,
    // one for the count of this index
    int index_size = ((curr_items+2) * sizeof(off_t));
    off_t *index;
    int index_ptr = 0;
    struct pst_debug_file_rec_m mfile_rec;
    struct pst_debug_file_rec_l lfile_rec;

    if (curr_items == 0) return;    // no items to write.

    index = (off_t*)xmalloc(index_size);
    memset(index, 0, index_size);   // valgrind, avoid writing uninitialized data
    file_pos += index_size;
    // write the index first, we will re-write it later, but
    // we want to allocate the space
    fwrite(index, index_size, 1, debug_fp);
    index[index_ptr++] = curr_items;

    item_ptr = item_head;
    while (item_ptr) {
        file_pos = ftell(debug_fp);
        index[index_ptr++] = file_pos;
        size = strlen(item_ptr->function) +
               strlen(item_ptr->file)     +
               strlen(item_ptr->text)     + 3; //for the three \0s
        if (buf) free(buf);
        buf = xmalloc(size+1);
        ptr = 0;
        funcname=ptr;
        ptr += sprintf(&(buf[ptr]), "%s", item_ptr->function)+1;
        filename=ptr;
        ptr += sprintf(&(buf[ptr]), "%s", item_ptr->file)+1;
        text=ptr;
        ptr += sprintf(&(buf[ptr]), "%s", item_ptr->text)+1;
        end=ptr;
        if (end > USHRT_MAX) { // bigger than can be stored in a short
            rec_type = 'L';
            fwrite(&rec_type, 1, sizeof(char), debug_fp);
            lfile_rec.type = item_ptr->type;
            lfile_rec.line = item_ptr->line;
            lfile_rec.funcname = funcname;
            lfile_rec.filename = filename;
            lfile_rec.text = text;
            lfile_rec.end = end;
            fwrite(&lfile_rec, sizeof(lfile_rec), 1, debug_fp);
        } else {
            rec_type = 'M';
            fwrite(&rec_type, 1, sizeof(char), debug_fp);
            mfile_rec.type = item_ptr->type;
            mfile_rec.line = item_ptr->line;
            mfile_rec.funcname = funcname;
            mfile_rec.filename = filename;
            mfile_rec.text = text;
            mfile_rec.end = end;
            fwrite(&mfile_rec, sizeof(mfile_rec), 1, debug_fp);
        }
        fwrite(buf, 1, ptr, debug_fp);
        if (buf) free(buf); buf = NULL;
        item_head = item_ptr->next;
        free(item_ptr->function);
        free(item_ptr->file);
        free(item_ptr->text);
        free(item_ptr);
        item_ptr = item_head;
    }
    curr_items = 0;
    index[index_ptr] = ftell(debug_fp);

    // we should now have a complete index
    fseek(debug_fp, index_pos, SEEK_SET);
    fwrite(index, index_size, 1, debug_fp);
    fseek(debug_fp, 0, SEEK_END);
    item_ptr = item_head = item_tail = NULL;
    free(index);
    if (buf) free(buf);
}


void pst_debug_write_msg(struct pst_debug_item *item, char *fmt, va_list *ap, int size) {
    struct pst_debug_file_rec_l lfile_rec;
    struct pst_debug_file_rec_m mfile_rec;
    unsigned char rec_type;
    int index_size = 3 * sizeof(off_t);
    off_t index[3];
    off_t index_pos, file_pos;
    char zero='\0';
    unsigned int end;
    if (!debug_fp) return;  // no file
    index[0] = 1; //only one item in this index
    index_pos = ftell(debug_fp);
    fwrite(index, index_size, 1, debug_fp);

    index[1] = ftell(debug_fp);

    if (size > USHRT_MAX) { // bigger than can be stored in a short
        rec_type = 'L';
        fwrite(&rec_type, 1, sizeof(char), debug_fp);
        lfile_rec.type = item->type;
        lfile_rec.line = item->line;
        lfile_rec.funcname = 0;
        lfile_rec.filename = strlen(item->function)+1;
        lfile_rec.text = lfile_rec.filename+strlen(item->file)+1;
        fwrite(&lfile_rec, sizeof(lfile_rec), 1, debug_fp);
    } else {
        rec_type = 'M';
        fwrite(&rec_type, 1, sizeof(char), debug_fp);
        mfile_rec.type = item->type;
        mfile_rec.line = item->line;
        mfile_rec.funcname = 0;
        mfile_rec.filename = strlen(item->function)+1;
        mfile_rec.text = mfile_rec.filename+strlen(item->file)+1;
        fwrite(&mfile_rec, sizeof(mfile_rec), 1, debug_fp);
    }
    file_pos = ftell(debug_fp);
    fwrite(item->function, strlen(item->function)+1, 1, debug_fp);
    fwrite(item->file, strlen(item->file)+1, 1, debug_fp);
    vfprintf(debug_fp, fmt, *ap);
    fwrite(&zero, 1, 1, debug_fp);

    end = ftell(debug_fp)-file_pos;

    index[2] = ftell(debug_fp);
    fseek(debug_fp, index_pos, SEEK_SET);
    fwrite(index, index_size, 1, debug_fp);
    if (size > USHRT_MAX) {
        fwrite(&rec_type, 1, sizeof(char), debug_fp);
        lfile_rec.end = end;
        fwrite(&lfile_rec, sizeof(lfile_rec), 1, debug_fp);
    } else {
        fwrite(&rec_type, 1, sizeof(char), debug_fp);
        mfile_rec.end = end;
        fwrite(&mfile_rec, sizeof(mfile_rec), 1, debug_fp);
    }
    fseek(debug_fp, 0, SEEK_END);
}


void pst_debug_write_hex(struct pst_debug_item *item, unsigned char *buf, size_t size, int col) {
    struct pst_debug_file_rec_l lfile_rec;
    unsigned char rec_type;
    int index_size = 3 * sizeof(off_t);
    off_t index_pos, file_pos, index[3];
    char zero='\0';
    if (!debug_fp) return;  // no file
    index[0] = 1; // only one item in this index run
    index[1] = 0; // valgrind, avoid writing uninitialized data
    index[2] = 0; // ""
    index_pos = ftell(debug_fp);
    fwrite(index, index_size, 1, debug_fp);
    index[1] = ftell(debug_fp);

    // always use the long
    rec_type = 'L';
    fwrite(&rec_type, 1, sizeof(char), debug_fp);
    lfile_rec.funcname = 0;
    lfile_rec.filename = strlen(item->function)+1;
    lfile_rec.text = lfile_rec.filename+strlen(item->file)+1;
    lfile_rec.end  = 0; // valgrind, avoid writing uninitialized data
    lfile_rec.line = item->line;
    lfile_rec.type = item->type;
    fwrite(&lfile_rec, sizeof(lfile_rec), 1, debug_fp);

    file_pos = ftell(debug_fp);
    fwrite(item->function, strlen(item->function)+1, 1, debug_fp);
    fwrite(item->file, strlen(item->file)+1, 1, debug_fp);

    pst_debug_hexdumper(debug_fp, buf, size, col, 0);
    fwrite(&zero, 1, 1, debug_fp);
    lfile_rec.end = ftell(debug_fp) - file_pos;

    index[2] = ftell(debug_fp);
    fseek(debug_fp, index_pos, SEEK_SET);
    fwrite(index, index_size, 1, debug_fp);
    fwrite(&rec_type, 1, sizeof(char), debug_fp);
    fwrite(&lfile_rec, sizeof(lfile_rec), 1, debug_fp);
    fseek(debug_fp, 0, SEEK_END);
}


void *xmalloc(size_t size) {
    void *mem = malloc(size);
    if (!mem) {
        fprintf(stderr, "xMalloc: Out Of memory [req: %ld]\n", (long)size);
        exit(1);
    }
    return mem;
}

