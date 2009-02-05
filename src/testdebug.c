#include "define.h"

#define BUF_SIZE 100000

int main(int argc, const char** argv) {
  char *x = xmalloc(BUF_SIZE); // 10k
  memset(x, '.', BUF_SIZE-1);
  x[BUF_SIZE-2]='P';
  x[BUF_SIZE-1]='\0';

  DEBUG_INIT("libpst.log");
  DEBUG_REGISTER_CLOSE();

  DEBUG_ENT("main");

  DEBUG_FILE(("%s", x));
  DEBUG_FILE(("This is an error %d\n", 4));

  DEBUG_RET();
  return 0;
}

