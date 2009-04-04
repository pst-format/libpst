
#include "define.h"

int process = 0, binary = 0;
pst_file pstfile;


void usage();
void usage()
{
    printf("usage: getidblock [options] filename id\n");
    printf("\tfilename - name of the file to access\n");
    printf("\tid - ID of the block to fetch (0 to fetch all) - can begin with 0x for hex\n");
    printf("\toptions\n");
    printf("\t\t-p\tProcess the block before finishing.\n");
    printf("\t\t\tView the debug log for information\n");
}


void dumper(uint64_t i_id);
void dumper(uint64_t i_id)
{
    char *buf = NULL;
    size_t readSize;
    pst_desc_ll *ptr;

    DEBUG_MAIN(("\n\n\nLooking at block index1 id %#"PRIx64"\n", i_id));

    if ((readSize = pst_ff_getIDblock_dec(&pstfile, i_id, &buf)) <= 0 || buf == 0) {
        DIE(("Error loading block\n"));
    }

    DEBUG_MAIN(("Printing block id %#"PRIx64", size %#"PRIx64"\n", i_id, (uint64_t)readSize));
    if (binary) {
        if (fwrite(buf, 1, readSize, stdout) != 0) {
            DIE(("Error occured during writing of buf to stdout\n"));
        }
    } else {
        printf("Block id %#"PRIx64", size %#"PRIx64"\n", i_id, (uint64_t)readSize);
        pst_debug_hexdumper(stdout, buf, readSize, 0x10, 0);
    }
    if (buf) free(buf);

    if (process) {
        DEBUG_MAIN(("Parsing block id %#"PRIx64"\n", i_id));
        ptr = pstfile.d_head;
        while (ptr) {
            if (ptr->assoc_tree && ptr->assoc_tree->i_id == i_id)
                break;
            if (ptr->desc && ptr->desc->i_id == i_id)
                break;
            ptr = pst_getNextDptr(ptr);
        }
        if (!ptr) {
            ptr = (pst_desc_ll *) pst_malloc(sizeof(pst_desc_ll));
            memset(ptr, 0, sizeof(pst_desc_ll));
            ptr->desc = pst_getID(&pstfile, i_id);
        }
        pst_item *item = pst_parse_item(&pstfile, ptr, NULL);
        if (item) pst_freeItem(item);
    }
}


void dump_desc(pst_desc_ll *ptr);
void dump_desc(pst_desc_ll *ptr)
{
    while (ptr) {
        DEBUG_MAIN(("\n\n\nLooking at block desc id %#"PRIx64"\n", ptr->d_id));
        if (ptr->desc       && ptr->desc->i_id)       dumper(ptr->desc->i_id);
        if (ptr->assoc_tree && ptr->assoc_tree->i_id) dumper(ptr->assoc_tree->i_id);
        if (ptr->child) dump_desc(ptr->child);
        ptr = ptr->next;
    }
}


int main(int argc, char* const* argv)
{
    // pass the id number to display on the command line
    char *fname, *sid;
    uint64_t i_id;
    int c;

    DEBUG_INIT("getidblock.log");
    DEBUG_REGISTER_CLOSE();
    DEBUG_ENT("main");

    while ((c = getopt(argc, argv, "bdp")) != -1) {
        switch (c) {
            case 'b':
                // enable binary output
                binary = 1;
                break;
            case 'p':
                // enable procesing of block
                process = 1;
                break;
            default:
                usage();
                exit(EXIT_FAILURE);
        }
    }

    if (optind + 1 >= argc) {
        // no more items on the cmd
        usage();
        exit(EXIT_FAILURE);
    }
    fname = argv[optind];
    sid   = argv[optind + 1];
    i_id  = (uint64_t)strtoll(sid, NULL, 0);

    DEBUG_MAIN(("Opening file\n"));
    memset(&pstfile, 0, sizeof(pstfile));
    if (pst_open(&pstfile, fname)) {
        DIE(("Error opening file\n"));
    }

    DEBUG_MAIN(("Loading Index\n"));
    if (pst_load_index(&pstfile) != 0) {
        DIE(("Error loading file index\n"));
    }

    if (i_id) {
        dumper(i_id);
    }
    else {
        pst_index_ll *ptr = pstfile.i_head;
        while (ptr) {
            dumper(ptr->i_id);
            ptr = ptr->next;
        }
        dump_desc(pstfile.d_head);
    }

    if (pst_close(&pstfile) != 0) {
        DIE(("pst_close failed\n"));
    }

    DEBUG_RET();
    return 0;
}

