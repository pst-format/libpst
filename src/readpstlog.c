
#include "common.h"

#include "define.h"

#define BUF_SIZE 4096

void usage();
size_t get(void * buf, int size, unsigned int count, FILE *fp);
int split_args(char *args, int **targ);
int is_in(int a, int *b, int c);

int main(int argc, char* const* argv) {
    int identity = 0;
    int level = 0;
    int x, ptr, stop=0, flag;
    char *fname, *buf, rec_type;
    unsigned char version;
    int *show_type=NULL, show_size=0;
    int *ex_type=NULL, ex_size=0;
    unsigned int funcname=0, filename=0, text=0, end=0, dtype=0, line=0;
    int c;
    char ch;
    FILE *fp;
    struct pst_debug_file_rec_m mfile_rec;
    struct pst_debug_file_rec_l lfile_rec;
    char format = 'D';  // default
    while ((c = getopt(argc, argv, "f:t:x:")) != -1) {
        ch = c;
        switch(ch) {
            case 'f':
                // change the output format
                format = toupper(optarg[0]);
                break;
            case 't':
                //change the type of statements shown
                show_size = split_args(optarg, &show_type);
                //      type = atoi(optarg);
                break;
            case 'x':
                // change the type of statements excluded
                ex_size = split_args(optarg, &ex_type);
                break;
        }
    }
    if (argc > optind) {
        fname = argv[optind++];
    } else {
        usage();
        exit(2);
    }

    fp = fopen(fname, "rb");
    if (fp == NULL) {
        printf("Error. couldn't open debug file\n");
        return 2;
    }
    if (get(&version, sizeof(char), 1, fp)==0) {
        printf("Error. could not read version byte from front of file");
        return 3;
    }

    if (version > DEBUG_VERSION) {
        printf("Version number is higher than the format I know about.");
        return 4;
    }

    buf = (char*) xmalloc(BUF_SIZE);

    while (!stop) {
        int64_t temp;
        if (fread(&temp, sizeof(int64_t), 1, fp)<=0) break;
        x = (int)temp;
        ptr = 0;
        if (x > 0) {
            int64_t i[x+1]; // plus 1 because we want to read the offset of the next index
            if (get(i, sizeof(int64_t), x+1, fp)==0) {
                // we have reached the end of the debug file
                printf("oh dear. we must now end\n");
                break;
            }
            while (ptr < x) {
                fseeko(fp, i[ptr++], SEEK_SET);
                get(&rec_type, 1, sizeof(char), fp);
                if (rec_type == 'L') {
                    get(&lfile_rec, sizeof(lfile_rec), 1, fp);
                    funcname=lfile_rec.funcname;
                    filename=lfile_rec.filename;
                    text = lfile_rec.text;
                    end = lfile_rec.end;
                    dtype = lfile_rec.type;
                    line = lfile_rec.line;
                } else if (rec_type == 'M') {
                    get(&mfile_rec, sizeof(mfile_rec), 1, fp);
                    funcname = mfile_rec.funcname;
                    filename = mfile_rec.filename;
                    text = mfile_rec.text;
                    end = mfile_rec.end;
                    dtype = mfile_rec.type;
                    line = mfile_rec.line;
                }
                if (dtype == DEBUG_FUNCENT_NO) level++;
                if ((show_type == NULL || is_in(dtype, show_type, show_size)) &&
                    (ex_type == NULL   || !is_in(dtype, ex_type, ex_size))) {
                    unsigned int c = 0;
                    flag = 0;
                    while (c < end) {
                        int ii = (level-1) * 4;
                        if (ii < 0) ii = 0;
                        if (ii > 64) ii = 64;
                        char indent[ii+1];
                        memset(indent, ' ', ii);
                        indent[ii] = '\0';
                        if (c + (BUF_SIZE-1) < end) {
                            get(buf, 1, BUF_SIZE-1, fp);
                            buf[BUF_SIZE-1] = '\0';
                            c += BUF_SIZE-1;
                        } else {
                            get(buf, 1, end-c, fp);
                            buf[end-c] = '\0';
                            c = end;
                        }
                        if (flag == 0) {
                            if (format == 'I') { // indented text format
                                char *b = buf+text;
                                identity++;
                                //printf("%s %d %s/%s[%d]: ", indent, identity, &buf[filename], &buf[funcname], line);
                                printf("%s %s/%s[%d]: ", indent, &buf[filename], &buf[funcname], line);
                                while (b) {
                                    char *p = strchr(b, '\n');
                                    if (p) {
                                        *p = '\0';
                                        printf("%s\n%s ", b, indent);
                                        b = p + 1;
                                    }
                                    else {
                                        printf("%s", b);
                                        b = NULL;
                                    }
                                }
                            }
                            else if (format == 'T') { // text format
                                printf("%s/%s[%d]: %s", &buf[filename], &buf[funcname], line, &buf[text]);
                            } else {
                                printf("Type: %d\nFile[line]: %s[%d]\nFunction:%s\nText:%s", dtype,
                                       &buf[filename], line, &buf[funcname], &buf[text]);
                            }
                            flag = 1;
                        } else {
                            if (format == 'I') {
                                char *b = buf;
                                while (b) {
                                    char *p = strchr(b, '\n');
                                    if (p) {
                                        *p = '\0';
                                        printf("%s\n%s ", b, indent);
                                        b = p + 1;
                                    }
                                    else {
                                        printf("%s", b);
                                        b = NULL;
                                    }
                                }
                            }
                            else printf("%s", buf);
                        }
                    }
                    printf("\n");
                }
                if (dtype == DEBUG_FUNCRET_NO) level--;
            }
            if (fseeko(fp, i[ptr], SEEK_SET)==-1) {
                printf("finished\n");
                break;
            }
        } else {
            printf("...no more items\n");
            break;
        }
    }
    free(buf);
    fclose(fp);
    if (format == 'I') printf("final indent level = %i\n", level);
    return 0;
}

size_t get(void *buf, int size, unsigned int count, FILE *fp) {
    size_t z;
    if ((z=fread(buf, size, count, fp)) < count) {
        printf("Read Failed! (size=%d, count=%d,z=%ld)\n", size, count, (long)z);
        exit(1);
    }
    return z;
}

void usage() {
    printf("readlog -t[show_type] -x[exclude_type] -f[format] filename\n");
    printf("\tformat:\n\t\tt: text log format\n");
    printf("\t\ti: indented text log format\n");
    printf("\tshow_type:\n\t\tcomma separated list of types to show "
       "[ie, 2,4,1,6]\n");
    printf("\texclude_type:\n\t\tcomma separated list of types to exclude "
       "[ie, 1,5,3,7]\n");
}


int split_args(char *args, int **targ) {
    int count = 1, *i, x, z;
    char *tmp = args, *y;
    if (*targ != NULL) {
      free(*targ);
    }
    // find the number of tokens in the string. Starting
    // from 1 cause there will always be one
    while ((tmp = strchr(tmp, ',')) != NULL) {
        tmp++; count++;
    }
    *targ = (int*)xmalloc(count * sizeof(int));
    i = *targ; // for convienience
    tmp = args;
    z = 0;
    for (x = 0; x < count; x++) {
        y = strtok(tmp, ",");
        tmp = NULL; // must be done after first call
        if (y != NULL) {
            i[x] = atoi(y);
            z++;
        }
    }
    return z;
}


// checks to see if the first arg is in the array of the second arg,
// the size of which is specified with the third arg. If the second
// arg is NULL, then it is obvious that it is not there.
int is_in(int a, int *b, int c){
    int d = 0;
    if (b == NULL || c == 0) { // no array or no items in array
        return 0;
    }
    while (d < c) {
        if (a == b[d]) return 1;
        d++;
    }
    return 0;
}
