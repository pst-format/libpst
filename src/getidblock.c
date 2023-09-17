
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
    printf("\t\t-b\tDump the blocks in binary to stdout.\n");
    printf("\t\t\tView the debug log for information\n");
}


void dumper(uint64_t i_id);
void dumper(uint64_t i_id)
{
    char *buf = NULL;
    size_t readSize;
    pst_desc_tree *ptr;

    DEBUG_INFO(("\n\n\nLooking at block index1 id %#" PRIx64 "\n", i_id));

    if ((readSize = pst_ff_getIDblock_dec(&pstfile, i_id, &buf)) <= 0 || buf == 0) {
        DIE(("Error loading block\n"));
    }

    DEBUG_INFO(("Printing block i_id %#" PRIx64 ", size %#zx\n", i_id, readSize));
    if (binary) {
        if (fwrite(buf, 1, readSize, stdout) != 0) {
            DIE(("Error occurred during writing of buf to stdout\n"));
        }
    } else {
        printf("Block id %#" PRIx64 ", size %#zx\n", i_id, readSize);
        pst_debug_hexdumper(stdout, buf, readSize, 0x10, 0);
    }
    if (buf) free(buf);

    if (process) {
        DEBUG_INFO(("Parsing block id %#" PRIx64 "\n", i_id));
        ptr = pstfile.d_head;
        while (ptr) {
            if (ptr->assoc_tree && ptr->assoc_tree->i_id == i_id)
                break;
            if (ptr->desc && ptr->desc->i_id == i_id)
                break;
            ptr = pst_getNextDptr(ptr);
        }
        if (!ptr) {
            ptr = (pst_desc_tree *) pst_malloc(sizeof(pst_desc_tree));
            memset(ptr, 0, sizeof(pst_desc_tree));
            ptr->desc = pst_getID(&pstfile, i_id);
        }
        pst_item *item = pst_parse_item(&pstfile, ptr, NULL);
        if (item) pst_freeItem(item);
    }
}


void dump_desc(pst_desc_tree *ptr, pst_desc_tree *parent);
void dump_desc(pst_desc_tree *ptr, pst_desc_tree *parent)
{
    while (ptr) {
        uint64_t parent_d_id = (parent) ? parent->d_id : 0;
        printf("Descriptor block d_id %#" PRIx64 " parent d_id %#" PRIx64 " children %" PRIi32 " desc.i_id=%#" PRIx64 ", assoc tree.i_id=%#" PRIx64 "\n",
            ptr->d_id, parent_d_id, ptr->no_child,
            (ptr->desc       ? ptr->desc->i_id       : (uint64_t)0),
            (ptr->assoc_tree ? ptr->assoc_tree->i_id : (uint64_t)0));
        if (ptr->desc       && ptr->desc->i_id)       dumper(ptr->desc->i_id);
        if (ptr->assoc_tree && ptr->assoc_tree->i_id) dumper(ptr->assoc_tree->i_id);
        if (ptr->child) dump_desc(ptr->child, ptr);
        ptr = ptr->next;
    }
}


int main(int argc, char* const* argv)
{
    // pass the id number to display on the command line
    char *fname, *sid;
    uint64_t i_id;
    int c;

    DEBUG_INIT("getidblock.log", NULL);
    DEBUG_ENT("main");

    while ((c = getopt(argc, argv, "bp")) != -1) {
        switch (c) {
            case 'b':
                // enable binary output
                binary = 1;
                break;
            case 'p':
                // enable processing of block
                process = 1;
                break;
            default:
                usage();
                DEBUG_CLOSE();
                exit(EXIT_FAILURE);
        }
    }

    if (optind + 1 >= argc) {
        // no more items on the cmd
        usage();
        DEBUG_CLOSE();
        exit(EXIT_FAILURE);
    }
    fname = argv[optind];
    sid   = argv[optind + 1];
    i_id  = (uint64_t)strtoll(sid, NULL, 0);

    DEBUG_INFO(("Opening file\n"));
    memset(&pstfile, 0, sizeof(pstfile));
    if (pst_open(&pstfile, fname, NULL)) {
        DIE(("Error opening file\n"));
    }

    DEBUG_INFO(("Loading Index\n"));
    if (pst_load_index(&pstfile) != 0) {
        pst_close(&pstfile);
        DIE(("Error loading file index\n"));
    }

    if (i_id) {
        dumper(i_id);
    }
    else {
        size_t i;
        for (i = 0; i < pstfile.i_count; i++) {
            dumper(pstfile.i_table[i].i_id);
        }
        dump_desc(pstfile.d_head, NULL);
    }

    if (pst_close(&pstfile) != 0) {
        DIE(("pst_close failed\n"));
    }

    DEBUG_RET();
    DEBUG_CLOSE();
    return 0;
}

