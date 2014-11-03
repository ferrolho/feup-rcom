#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "Aplication.h"
#include "CLI.h"

static void printUsage(char* argv0);
static int procArgs(int argc, char** argv);
static void printArguments(ConnnectionMode mode, char* port, char* file);

int main(int argc, char** argv) {
	if (argc != 1 && argc != 4) {
		printf("ERROR: Wrong number of arguments.\n");
		printUsage(argv[0]);
		return 1;
	}

	if (argc == 1)
		startCLI();
	else if (argc == 4)
		procArgs(argc, argv);

	return 0;
}

static void printUsage(char* argv0) {
	printf("Usage: %s\n", argv0);
	printf("       %s <send|receive> <port> <file>\n", argv0);
}

static int procArgs(int argc, char** argv) {
	ConnnectionMode mode;
	char *port, *file;

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

	printArguments(mode, port, file);

	initApplicationLayer(port, mode, B38400, file);

	return 0;
}

static void printArguments(ConnnectionMode mode, char* port, char* file) {
	printf("-------------------------\n");
	switch (mode) {
	case SEND:
		printf("- Mode: Send.\t\t-\n");
		break;
	case RECEIVE:
		printf("- Mode: Receive.\t-\n");
		break;
	default:
		printf("- ERROR: Mode: Default.\t-\n");
		break;
	}
	printf("- Port: %s.\t-\n", port);
	printf("- File: %s.\t-\n", file);
	printf("-------------------------\n");
}
