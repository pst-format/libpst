#include "define.h"

#define OUT_BUF 20

int main(int argc, char* const* argv)
{
    pst_file pstfile;
    size_t i;
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
    DEBUG_INIT("dumpblocks.log", NULL);
    DEBUG_ENT("main");

    printf("Opening file %s\n", file);
    if (pst_open(&pstfile, file, NULL)) {
        printf("Failed to open file %s\n", file);
        DEBUG_CLOSE();
        exit(1);
    }

    printf("Reading Indexes\n");
    if (pst_load_index(&pstfile)) {
        printf("Failed to load indexes in file %s\n", argv[1]);
        pst_close(&pstfile);
        DEBUG_CLOSE();
        exit(1);
    }

    if (outdir != NULL)
        if (chdir(outdir)) {
            printf("Failed to change into directory %s\n", outdir);
            pst_close(&pstfile);
            DEBUG_CLOSE();
            exit(1);
        }

    outname = (char *) pst_malloc(OUT_BUF);
    printf("Saving blocks\n");
    for (i = 0; i < pstfile.i_count; i++) {
        pst_index_ll *ptr = &pstfile.i_table[i];
        size_t c = pst_ff_getIDblock_dec(&pstfile, ptr->i_id, &buf);
        if (c) {
            snprintf(outname, OUT_BUF, "%#" PRIx64, ptr->i_id);
            if ((fp = fopen(outname, "wb")) == NULL) {
                printf("Failed to open file %s\n", outname);
                continue;
            }
            pst_fwrite(buf, 1, c, fp);
            fclose(fp);
        } else {
            printf("Failed to read block i_id %#" PRIx64 "\n", ptr->i_id);
        }
    }
    free(outname);
    pst_close(&pstfile);
    DEBUG_RET();
    DEBUG_CLOSE();
    return 0;
}
