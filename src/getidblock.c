#include <stdio.h>
#include <string.h>

#ifndef __GNUC__
# include "XGetopt.h"
#endif

#ifndef _WIN32
# include <unistd.h>
#endif

#include "define.h"
#include "libpst.h"

static void usage();

int main(int argc, char ** argv) {
  // pass the id number to display on the command line
  char *fname, *sid;
  pst_file pstfile;
  unsigned int id;
  int decrypt = 0, process = 0, binary = 0, c;
  unsigned char *buf = NULL;
  size_t readSize;
  pst_item *item;
  pst_desc_ll* ptr;

  DEBUG_INIT("getidblock.log");
  DEBUG_REGISTER_CLOSE();
  DEBUG_ENT("main");

  while ((c = getopt(argc, argv, "bdp")) != -1) {
	switch (c) {
	case 'b':
	  // enable binary output
	  binary = 1;
	  break;
	case 'd':
	  //enable decrypt
	  decrypt = 1;
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

  if (optind+1 >= argc) {
	// no more items on the cmd
	usage();
	exit(EXIT_FAILURE);
  }
  fname = argv[optind];
  sid = argv[optind+1];
  id = (unsigned int)strtol(sid, NULL, 0);

  DEBUG_MAIN(("Opening file\n"));
  memset(&pstfile, 0, sizeof(pstfile));
  if (pst_open(&pstfile, fname, "r")!=0) {
	DIE(("Error opening file\n"));
  }

  DEBUG_MAIN(("Loading Index\n"));
  if (pst_load_index(&pstfile) != 0) {
	DIE(("Error loading file index\n"));
  }

  //  if ((ptr = pst_getID(&pstfile, id)) == NULL) {
  //	DIE(("id not found [%#x]\n", id));
  //  }

  DEBUG_MAIN(("Loading block\n"));

  if ((readSize = pst_ff_getIDblock(&pstfile, id, &buf)) <= 0 || buf == NULL) {
	//	if ((readSize = pst_read_block_size(&pstfile, ptr->offset, ptr->size, &buf, 1, 1)) < ptr->size) {
	DIE(("Error loading block\n"));
  }
  if (binary==0) printf("Block %#x, size %#x[%i]\n",id, (unsigned int)readSize, (int) readSize);

  if (decrypt!=0)
	if (pst_decrypt(buf, readSize, (int)pstfile.encryption) != 0) {
	  DIE(("Error decrypting block\n"));
	}

  DEBUG_MAIN(("Printing block... [id %#x, size %#x]\n", id, readSize));
  if (binary==0) {
	pst_debug_hexdumper(stdout, buf, readSize, 0x10, 0);
  } else {
	if (fwrite(buf, 1, readSize, stdout) != 0) {
	  DIE(("Error occured during writing of buf to stdout\n"));
	}
  }
  free(buf);

  if (process!=0) {
	DEBUG_MAIN(("Parsing block...\n"));
	ptr = pstfile.d_head;
	while(ptr != NULL) {
	  if (ptr->list_index != NULL && ptr->list_index->id == id)
	break;
	  if (ptr->desc != NULL && ptr->desc->id == id)
	break;
	  ptr = pst_getNextDptr(ptr);
	}
	if (ptr == NULL) {
	  ptr = (pst_desc_ll*)xmalloc(sizeof(pst_desc_ll));
	  ptr->desc = pst_getID(&pstfile, id);
	  ptr->list_index = NULL;
	}
	if (ptr != NULL) {
	  if ((item = pst_parse_item(&pstfile, ptr)) != NULL)
	pst_freeItem(item);
	} else {
	  DEBUG_MAIN(("item not found with this ID\n"));
	  printf("Cannot find the owning Record of this ID. Cannot parse\n");
	}
  }

  if(pst_close(&pstfile)!=0) {
	DIE(("pst_close failed\n"));
  }

  DEBUG_RET();
  return 0;
}

void usage() {
  printf("usage: getidblock [options] filename id\n");
  printf("\tfilename - name of the file to access\n");
  printf("\tid - ID of the block to fetch - can begin with 0x for hex\n");
  printf("\toptions\n");
  printf("\t\t-d\tDecrypt the block before printing\n");
  printf("\t\t-p\tProcess the block before finishing.\n");
  printf("\t\t\tView the debug log for information\n");
}
