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

	return 0;
}

void printUsage(char* argv0) {
    printf("Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv0);
}


