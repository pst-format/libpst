
char * base64_encode(void *data, size_t size);
char * base64_encode_multiple(void *data, size_t size, int *line_count);
void hexdump(char *hbuf, int start, int stop, int ascii);
