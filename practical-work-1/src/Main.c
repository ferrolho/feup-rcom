#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "Aplication.h"
#include "CLI.h"
#include "Utilities.h"

const int DEFAULT_BAUDRATE = B38400;
const int DEFAULT_MESSAGE_DATA_MAX_SIZE = 512;
const int DEFAULT_NUM_RETRIES = 3;
const int DEFAULT_TIMEOUT = 3;

static void printUsage(char* argv0);
static int procArgs(int argc, char** argv);

int main(int argc, char** argv) {
	if (argc != 1 && argc != 5) {
		printf("ERROR: Wrong number of arguments.\n");
		printUsage(argv[0]);
		return 1;
	}

	if (argc == 1)
		startCLI();
	else if (argc == 5)
		procArgs(argc, argv);

	return 0;
}

static void printUsage(char* argv0) {
	printf("Usage: %s\n", argv0);
	printf("       %s <send|receive> <port> <file> <debugging>\n", argv0);
}

static int procArgs(int argc, char** argv) {
	ConnnectionMode mode;
	char *port, *file, *debug;

	if (strncmp(argv[1], "send", strlen("send")) == 0)
		mode = SEND;
	else if (strncmp(argv[1], "receive", strlen("receive")) == 0)
		mode = RECEIVE;
	else {
		printf(
				"ERROR: Neither send nor receive specified. Did you misspell something?\n");
		return -1;
	}

	port = argv[2];
	file = argv[3];
	debug = argv[4];

	if (strcmp(debug, "on") == 0) {
		DEBUG_MODE = TRUE;
		printf("! DEBUGGING ON !\n");
	}

	initApplicationLayer(port, mode, DEFAULT_BAUDRATE,
			DEFAULT_MESSAGE_DATA_MAX_SIZE, DEFAULT_NUM_RETRIES, DEFAULT_TIMEOUT,
			file);

	return 0;
}
