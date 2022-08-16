#include "define.h"


struct pst_debug_func {
    char * name;
    struct pst_debug_func *next;
};


#define NUM_COL 32
#define MAX_DEPTH 32

static struct pst_debug_func *func_head = NULL;
static int func_depth = 0;
static int pst_debuglevel = 0;
static char indent[MAX_DEPTH*4+1];
static FILE *debug_fp = NULL;
#ifdef HAVE_SEMAPHORE_H
    static sem_t* debug_mutex = NULL;
#endif


void pst_debug_setlevel(int level)
{
  pst_debuglevel = level;
}

void pst_debug_lock()
{
    #ifdef HAVE_SEMAPHORE_H
        if (debug_mutex) sem_wait(debug_mutex);
    #endif
}


void pst_debug_unlock()
{
    #ifdef HAVE_SEMAPHORE_H
        if (debug_mutex) sem_post(debug_mutex);
    #endif
}


void pst_debug_init(const char* fname, void* output_mutex) {
    #ifdef HAVE_SEMAPHORE_H
        debug_mutex = (sem_t*)output_mutex;
    #endif
    memset(indent, ' ', MAX_DEPTH*4);
    indent[MAX_DEPTH*4] = '\0';
    if (debug_fp) pst_debug_close();
    if (!fname) return;
    if ((debug_fp = fopen(fname, "wb")) == NULL) {
        fprintf(stderr, "Opening of file %s failed: %d: %s\n", fname, errno, strerror(errno));
        exit(1);
    }
}


void pst_debug_func(int level, const char* function) {
    if (pst_debuglevel > level) return;
    struct pst_debug_func *func_ptr = pst_malloc (sizeof(struct pst_debug_func));
    func_ptr->name = strdup(function);
    func_ptr->next = func_head;
    func_head = func_ptr;
    func_depth++;
}


void pst_debug_func_ret(int level) {
    if (pst_debuglevel > level) return;
    //remove the head item
    struct pst_debug_func *func_ptr = func_head;
    if (func_head) {
        func_head = func_head->next;
        free(func_ptr->name);
        free(func_ptr);
        func_depth--;
    } else {
        DIE(("function list is empty!\n"));
    }
}


static void pst_debug_info(int level, int line, const char* file);
static void pst_debug_info(int level, int line, const char* file) {
    if (pst_debuglevel > level) return;
    int le = (func_depth > MAX_DEPTH) ? MAX_DEPTH : func_depth;
    if (le > 0) le--;
    char *func = (func_head ? func_head->name : "No Function");
    pst_debug_lock();
    fprintf(debug_fp, "%06d %.*s%s %s(%d) ", getpid(), le*4, indent, func, file, line);
}


void pst_debug(int level, int line, const char* file, const char *fmt, ...) {
    if (pst_debuglevel > level) return;
    if (debug_fp) {
        pst_debug_info(level, line, file);
        va_list ap;
        va_start(ap,fmt);
        vfprintf(debug_fp, fmt, ap);
        va_end(ap);
        fflush(debug_fp);
        pst_debug_unlock();
    }
}


void pst_debug_hexdump(int level, int line, const char *file, const char *buf, size_t size, int cols, int delta) {
    if (pst_debuglevel > level) return;
    if (debug_fp) {
        pst_debug_info(level, line, file);
        pst_debug_hexdumper(debug_fp, buf, size, cols, delta);
        pst_debug_unlock();
    }
}


void pst_debug_hexdumper(FILE *out, const char *buf, size_t size, int cols, int delta) {
    int le = (func_depth > MAX_DEPTH) ? MAX_DEPTH : func_depth;
    size_t off = 0, toff;
    int count = 0;

    if (!out) return;   // no file

    if (cols == -1) cols = NUM_COL;
    fprintf(out, "\n");
    while (off < size) {
        fprintf(out, "%06d %.*s%06"PRIx64"\t:", getpid(), le*4, indent, (int64_t)(off+delta));
        toff = off;
        while (count < cols && off < size) {
            fprintf(out, "%02hhx ", (unsigned char)buf[off]);
            off++; count++;
        }
        off = toff;
        while (count < cols) {
            // only happens at end of block to pad the text over to the text column
            fprintf(out, "   ");
            count++;
        }
        count = 0;
        fprintf(out, ":");
        while (count < cols && off < size) {
            fprintf(out, "%c", isgraph(buf[off])?buf[off]:'.');
            off++; count ++;
        }

        fprintf(out, "\n");
        count=0;
    }

    fprintf(out, "\n");
    fflush(out);
}


void pst_debug_close(void) {
    while (func_head) {
        struct pst_debug_func *func_ptr = func_head;
        func_head = func_head->next;
        free(func_ptr->name);
        free(func_ptr);
    }
    if (debug_fp) fclose(debug_fp);
    debug_fp = NULL;
}


void *pst_malloc(size_t size) {
    void *mem = malloc(size);
    if (!mem) {
        fprintf(stderr, "pst_malloc: Out Of memory [req: %ld]\n", (long)size);
        exit(1);
    }
    return mem;
}


void *pst_realloc(void *ptr, size_t size) {
    void *mem = realloc(ptr, size);
    if (!mem) {
        fprintf(stderr, "pst_realloc: Out Of memory [req: %ld]\n", (long)size);
        exit(1);
    }
    return mem;
}
