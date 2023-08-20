#include "define.h"

#include <fcntl.h>
#include <iostream>
#include <vector>
using namespace std;

unsigned char comp_enc [] =
  { 0x47, 0xf1, 0xb4, 0xe6, 0x0b, 0x6a, 0x72, 0x48,
	0x85, 0x4e, 0x9e, 0xeb, 0xe2, 0xf8, 0x94, 0x53, /*0x0f*/
	0xe0, 0xbb, 0xa0, 0x02, 0xe8, 0x5a, 0x09, 0xab,
	0xdb, 0xe3, 0xba, 0xc6, 0x7c, 0xc3, 0x10, 0xdd, /*0x1f*/
	0x39, 0x05, 0x96, 0x30, 0xf5, 0x37, 0x60, 0x82,
	0x8c, 0xc9, 0x13, 0x4a, 0x6b, 0x1d, 0xf3, 0xfb, /*0x2f*/
	0x8f, 0x26, 0x97, 0xca, 0x91, 0x17, 0x01, 0xc4,
	0x32, 0x2d, 0x6e, 0x31, 0x95, 0xff, 0xd9, 0x23, /*0x3f*/
	0xd1, 0x00, 0x5e, 0x79, 0xdc, 0x44, 0x3b, 0x1a,
	0x28, 0xc5, 0x61, 0x57, 0x20, 0x90, 0x3d, 0x83, /*0x4f*/
	0xb9, 0x43, 0xbe, 0x67, 0xd2, 0x46, 0x42, 0x76,
	0xc0, 0x6d, 0x5b, 0x7e, 0xb2, 0x0f, 0x16, 0x29, /*0x5f*/
	0x3c, 0xa9, 0x03, 0x54, 0x0d, 0xda, 0x5d, 0xdf,
	0xf6, 0xb7, 0xc7, 0x62, 0xcd, 0x8d, 0x06, 0xd3, /*0x6f*/
	0x69, 0x5c, 0x86, 0xd6, 0x14, 0xf7, 0xa5, 0x66,
	0x75, 0xac, 0xb1, 0xe9, 0x45, 0x21, 0x70, 0x0c, /*0x7f*/
	0x87, 0x9f, 0x74, 0xa4, 0x22, 0x4c, 0x6f, 0xbf,
	0x1f, 0x56, 0xaa, 0x2e, 0xb3, 0x78, 0x33, 0x50, /*0x8f*/
	0xb0, 0xa3, 0x92, 0xbc, 0xcf, 0x19, 0x1c, 0xa7,
	0x63, 0xcb, 0x1e, 0x4d, 0x3e, 0x4b, 0x1b, 0x9b, /*0x9f*/
	0x4f, 0xe7, 0xf0, 0xee, 0xad, 0x3a, 0xb5, 0x59,
	0x04, 0xea, 0x40, 0x55, 0x25, 0x51, 0xe5, 0x7a, /*0xaf*/
	0x89, 0x38, 0x68, 0x52, 0x7b, 0xfc, 0x27, 0xae,
	0xd7, 0xbd, 0xfa, 0x07, 0xf4, 0xcc, 0x8e, 0x5f, /*0xbf*/
	0xef, 0x35, 0x9c, 0x84, 0x2b, 0x15, 0xd5, 0x77,
	0x34, 0x49, 0xb6, 0x12, 0x0a, 0x7f, 0x71, 0x88, /*0xcf*/
	0xfd, 0x9d, 0x18, 0x41, 0x7d, 0x93, 0xd8, 0x58,
	0x2c, 0xce, 0xfe, 0x24, 0xaf, 0xde, 0xb8, 0x36, /*0xdf*/
	0xc8, 0xa1, 0x80, 0xa6, 0x99, 0x98, 0xa8, 0x2f,
	0x0e, 0x81, 0x65, 0x73, 0xe4, 0xc2, 0xa2, 0x8a, /*0xef*/
	0xd4, 0xe1, 0x11, 0xd0, 0x08, 0x8b, 0x2a, 0xf2,
	0xed, 0x9a, 0x64, 0x3f, 0xc1, 0x6c, 0xf9, 0xec}; /*0xff*/

int main(int argc, char* const* argv) {
	if (argc < 4) {
		printf("usage: %s filename.pst integer-delta search-string\n", argv[0]);
		return 0;
	}
	int  fd = open(argv[1], O_RDONLY);
	int   d = atoi(argv[2]);
	string search(argv[3]);
	printf("using file %s with delta %d looking for %s\n", argv[1], d, argv[3]);
	if (fd >= 0) {
		struct stat st;
		fstat(fd, &st);
		off_t size = st.st_size;
		vector <char> buf(size);
		size_t s = read(fd, &buf[0], size);
		pst_debug_hexdumper(stdout, &buf[0], s, 16, 0);
		printf("\n\n dump decrypted data \n");
		for (off_t i=0; i<size; i++) {
			buf[i] = comp_enc[(unsigned char)buf[i]];
		}
		pst_debug_hexdumper(stdout, &buf[0], s, 16, 0);
		close(fd);
	}
	return 0;
}
