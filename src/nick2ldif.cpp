/*

Copyright (c) 2004 Carl Byington - 510 Software Group, released under
the GPL version 2 or any later version at your choice available at
https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

*/

#include <iostream>

#include "define.h"

char *ldap_base  = NULL;
char *ldap_org	 = NULL;
char *ldap_class = NULL;

using namespace std;

int main(int argc, char* const* argv) {
	char c;
	char *temp;
	while ((c = getopt(argc, argv, "b:c:"))!= -1) {
		switch (c) {
		case 'b':
			ldap_base = optarg;
			temp = strchr(ldap_base, ',');
			if (temp) {
				*temp = '\0';
				ldap_org = strdup(ldap_base);
				*temp = ',';
			}
			break;
		case 'c':
			ldap_class = optarg;
			break;
		default:
			break;
		}
	}

	const int LINE_SIZE = 2000;
	char line[LINE_SIZE];
	while (!cin.eof()) {
		cin.getline(line, LINE_SIZE);
		int n = strlen(line);
		if (!n) continue;
        if (strncmp(line, "alias", 5) != 0) continue;   // not alias
		char *f = line + 6; 	// skip alias keyword
		char *e;
		if (*f == '"') {
			f++;
			e = strchr(f, '"');
		}
		else {
			e = strchr(f, ' ');
		}
		if (!e) continue;
		*e = '\0';
		char *m = e+1;
		while (*m == ' ') m++;
		if (*m != '\0') {
			char cn[LINE_SIZE], givenName[LINE_SIZE], sn[LINE_SIZE];
			snprintf(cn, sizeof(cn), "%s", f);
            char *ff = strchr(f, ' ');
            if (ff) {
                strncpy(givenName, ff+1, sizeof(givenName)-1);
                *ff = '\0';
                strncpy(sn, f, sizeof(sn)-1);
            }
            else {
                strcpy(givenName, cn);
                strcpy(sn, cn);
            }
			printf("dn: cn=%s, %s\n", cn, ldap_base);
			printf("cn: %s\n", cn);
            printf("givenName: %s\n", givenName);
			printf("sn: %s\n", sn);
			printf("mail: %s\n", m);
			printf("objectClass: %s\n\n", ldap_class);
		}
	}
}
