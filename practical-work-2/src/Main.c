#include <stdio.h>
#include "URL.h"
#include "FTP.h"

static void printUsage(char* argv0);

int main(int argc, char** argv) {

	if (argc != 2) {
			printf("ERROR: Wrong number of arguments.\n");
			printUsage(argv[0]);
			return 1;
		}

	url url;
	// TODO check functions with integer returns
	initURL(&url);
	parseURL(&url, argv[1]);
	getIpByHost(&url);

	printf("\n%s\n", url.ip);

	ftp ftp;
	ftpConnect(&ftp, url.ip, 21);

	const char* user = strlen(url.user) ? url.user : "anonymous";
	const char* password = strlen(url.password) ? url.password : "anonymous@fe.up.pt";

	//TODO create a function read and send to organize code in login and connect
	ftpLogin(&ftp, user, password);

	return 0;
}

void printUsage(char* argv0) {
    printf("Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv0);
}
