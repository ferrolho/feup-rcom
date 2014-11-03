#include "CLI.h"

#include <stdio.h>
#include <stdlib.h>
#include "Aplication.h"
#include "ConnectionMode.h"
#include "DataLink.h"

void startCLI() {
	printf("===========================\n");
	printf("= RCOM - practical work 1 =\n");
	printf("===========================\n");
	printf("\n");
	printf("Choose what to do:\n");
	printf("    1. Send    2. Receive\n");
	printf("\n");

	ConnnectionMode mode = getIntInput(1, 2) - 1;
	printf("\n");

	int intBaudrate;
	do {
		printf(
				"What Baud rate should be used? { 0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800 } ");
		intBaudrate = getIntInput(0, 460800);
	} while (intBaudrate == -1);
	int baudrate = getBaudrate(intBaudrate);
	printf("\n");

	printf("What port - x - should be used? (/dev/ttySx) ");
	int portNum = getIntInput(0, 9);

	char port[20] = "/dev/ttySx";
	port[9] = '0' + portNum;
	printf("Using port: %s\n", port);
	printf("\n");

	switch (mode) {
	case SEND:
		printf("What file do you wish to transfer? ");
		break;
	case RECEIVE:
		printf("Which name should the received file have? ");
		break;
	default:
		printf("Unexpected error: Invalid mode.\n");
		break;
	}

	char* file = getStringInput();

	initApplicationLayer(port, mode, baudrate, file);
}

int getIntInput(int start, int end) {
	int input;

	int done = 0;
	while (!done) {
		printf("$ ");

		if (scanf("%d", &input) == 1 && start <= input && input <= end)
			done = 1;
		else
			printf("Invalid input. Try again:\n");

		clearInputBuffer();
	}

	return input;
}

char* getStringInput() {
	char* input = (char*) malloc(256 * sizeof(char));

	int done = 0;
	while (!done) {
		printf("$ ");

		if (scanf("%s", input) == 1)
			done = 1;
		else
			printf("Invalid input. Try again:\n");

		clearInputBuffer();
	}

	return input;
}

void clearInputBuffer() {
	int c;
	while ((c = getchar()) != '\n' && c != EOF)
		;
}

const int PROGRESS_BAR_LENGTH = 51;

void printProgressBar(float current, float total) {
	float percentage = 100.0 * current / total;

	printf("\rCompleted: %6.2f%% [", percentage);

	int i, len = PROGRESS_BAR_LENGTH;
	int pos = percentage * len / 100.0;

	for (i = 0; i < len; i++)
		i <= pos ? printf("=") : printf(" ");

	printf("]");

	fflush(stdout);
}
