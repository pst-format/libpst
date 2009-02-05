#include "define.h"

#define OUT_BUF 20

int main(int argc, char* const* argv)
{
    pst_file pstfile;
    pst_index_ll *ptr;
    char *outdir = NULL, *file = NULL, *outname = NULL;
    char *buf = NULL;
    int c;
    FILE *fp;

    while ((c = getopt(argc, argv, "o:")) != -1) {
        switch (c) {
            case 'o':
                outdir = optarg;
                break;
            default:
                printf("Unknown switch %c\n", c);
        }
    }
    if (optind < argc) {
        file = argv[optind];
    } else {
        printf("Usage: dumpblocks [options] pstfile\n");
        printf("\tcopies the datablocks from the pst file into separate files\n");
        printf("Options: \n");
        printf("\t-o target\tSpecify the output directory\n");
        exit(1);
    }
    DEBUG_INIT("dumpblocks.log");
    DEBUG_REGISTER_CLOSE();
    DEBUG_ENT("main");

    printf("Opening file %s\n", file);
    if (pst_open(&pstfile, file)) {
        printf("Failed to open file %s\n", file);
        exit(1);
    }

    printf("Reading Indexes\n");
    if (pst_load_index(&pstfile)) {
        printf("Failed to load indexes in file %s\n", argv[1]);
        exit(1);
    }

    if (outdir != NULL)
        if (chdir(outdir)) {
            printf("Failed to change into directory %s\n", outdir);
            exit(1);
        }

    ptr = pstfile.i_head;
    outname = (char *) xmalloc(OUT_BUF);
    printf("Saving blocks\n");
    while (ptr != NULL) {
        /*    if (pstfile.encryption == PST_ENC) {
           c = pst_ff_getIDblock_dec(&pstfile, ptr->id, buf);
           } else { */
        if ((ptr->id & 0x02) == 0 && pstfile.encryption == PST_ENC) {
            c = pst_ff_getIDblock_dec(&pstfile, ptr->id, &buf);
        } else {
            c = pst_ff_getIDblock(&pstfile, ptr->id, &buf);
        }

        if (c > 0) {
            snprintf(outname, OUT_BUF, "%#"PRIx64, ptr->id);
            if ((fp = fopen(outname, "wb")) == NULL) {
                printf("Failed to open file %s\n", outname);
                continue;
            }
            pst_fwrite(buf, 1, c, fp);
            fclose(fp);
        } else {
            printf("Failed to read block id %#"PRIx64"\n", ptr->id);
        }
        ptr = ptr->next;
    }
    pst_close(&pstfile);
    DEBUG_RET();
    return 0;
}
