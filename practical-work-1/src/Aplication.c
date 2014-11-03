#include "Aplication.h"

#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "DataLink.h"
#include "CLI.h"
#include "ConnectionMode.h"
#include "ControlPackageType.h"
#include "ParameterType.h"
#include "Utilities.h"

int DEBUG_MODE = FALSE;

ApplicationLayer* al;

int initApplicationLayer(const char* port, ConnnectionMode mode, int baudrate,
		int messageDataMaxSize, int numRetries, int timeout, char* file) {
	al = (ApplicationLayer*) malloc(sizeof(ApplicationLayer));

	al->fd = openSerialPort(port);
	if (al->fd < 0) {
		printf("ERROR: Serial port could not be open.\n");
		return 0;
	}
	al->mode = mode;
	al->fileName = file;

	if (!initLinkLayer(port, mode, baudrate, messageDataMaxSize, numRetries,
			timeout)) {
		printf("ERROR: Could not initialize Link Layer.\n");
		return 0;
	}

	printConnectionInfo();

	startConnection();

	if (!closeSerialPort(al->fd))
		return 0;

	printConnectionStatistics();

	return 1;
}

void printConnectionInfo() {
	printf("===================\n");
	printf("= Connection info =\n");
	printf("===================\n");

	switch (ll->mode) {
	case SEND:
		printf("# Mode: Send\n");
		break;
	case RECEIVE:
		printf("# Mode: Receive\n");
		break;
	default:
		printf("# ERROR: Mode: Default\n");
		break;
	}

	printf("# Baud rate: %d\n", ll->baudRate);
	printf("# Message data max. size: %d\n", ll->messageDataMaxSize);
	printf("# Max. no. retries: %d\n", ll->numTries - 1);
	printf("# Time-out interval: %d\n", ll->timeout);
	printf("# Port: %s\n", ll->port);
	printf("# File: %s\n", al->fileName);

	printf("\n");
}

int startConnection() {
	switch (al->mode) {
	case RECEIVE:
		if (DEBUG_MODE)
			printf("Starting connection in RECEIVE mode.\n");
		receiveFile();
		break;
	case SEND:
		if (DEBUG_MODE)
			printf("Starting connection in SEND mode.\n");
		sendFile();
		break;
	default:
		break;
	}

	return 1;
}

void printConnectionStatistics() {
	printf("\n");
	printf("=========================\n");
	printf("= Connection statistics =\n");
	printf("=========================\n");

	printf("# Sent messages: %d\n", ll->stats->sentMessages);
	printf("# Received messages: %d\n", ll->stats->receivedMessages);
	printf("#\n");
	printf("# Time-outs: %d\n", ll->stats->timeouts);
	printf("#\n");
	printf("# Sent RR: %d\n", ll->stats->numSentRR);
	printf("# Received RR: %d\n", ll->stats->numReceivedRR);
	printf("#\n");
	printf("# Sent REJ: %d\n", ll->stats->numSentREJ);
	printf("# Received REJ: %d\n", ll->stats->numReceivedREJ);
}

int sendFile() {
	// open file to be sent
	FILE* file = fopen(al->fileName, "rb");
	if (!file) {
		printf("ERROR: Could not open file to be sent.\n");
		return 0;
	} else if (DEBUG_MODE)
		printf("Successfully opened file to be sent.\n");

	// open connection
	int fd = llopen(ll->mode);
	if (fd <= 0)
		return 0;

	// get size of file to be sent
	int fileSize = getFileSize(file);
	char fileSizeBuf[sizeof(int) * 3 + 2];
	snprintf(fileSizeBuf, sizeof fileSizeBuf, "%d", fileSize);

	// send start control package
	if (!sendControlPackage(al->fd, CTRL_PKG_START, fileSizeBuf, al->fileName))
		return 0;

	// allocate space for file buffer
	char* fileBuf = malloc(MAX_SIZE);

	if (DEBUG_MODE)
		printf("*** Starting file chunks transfer. ***\n");

	// read file chunks
	ui readBytes = 0, writtenBytes = 0, i = 0;
	while ((readBytes = fread(fileBuf, sizeof(char), MAX_SIZE, file)) > 0) {
		// send those chunks inside data packages
		if (!sendDataPackage(fd, (i++) % 255, fileBuf, readBytes)) {
			free(fileBuf);
			return 0;
		}

		// reset file buffer
		fileBuf = memset(fileBuf, 0, MAX_SIZE);

		// increment no. of written bytes
		writtenBytes += readBytes;

		printProgressBar(writtenBytes, fileSize);
	}
	printf("\n\n");

	if (DEBUG_MODE)
		printf("*** File chunks transfer complete. ***\n");

	free(fileBuf);

	if (fclose(file) != 0) {
		printf("ERROR: Unable to close file.\n");
		return 0;
	}

	if (!sendControlPackage(fd, CTRL_PKG_END, "0", ""))
		return 0;

	if (!llclose(al->fd, ll->mode))
		return 0;

	printf("\n");
	printf("File successfully transferred.\n");

	return 1;
}

int receiveFile() {
	int fd = llopen(ll->mode);
	if (fd <= 0)
		return 0;

	// TODO create struct to carry these
	int controlStart, fileSize;
	char* fileName;

	if (DEBUG_MODE)
		printf("Waiting for START control package.\n");
	if (!receiveControlPackage(al->fd, &controlStart, &fileSize, &fileName))
		return 0;

	if (controlStart != CTRL_PKG_START) {
		printf(
				"ERROR: Control package received but its control field - %d - is not C_PKG_START",
				controlStart);
		return 0;
	}

	// create output file
	FILE* outputFile = fopen(al->fileName, "wb");
	if (outputFile == NULL) {
		printf("ERROR: Could not create output file.\n");
		return 0;
	}

	printf("\n");
	printf("Created output file: %s\n", al->fileName);
	printf("Expected file size: %d (bytes)\n", fileSize);
	printf("\n");

	if (DEBUG_MODE)
		printf("*** Starting file chunks transfer. ***\n");

	int fileSizeReadSoFar = 0, N = -1;
	while (fileSizeReadSoFar != fileSize) {
		int lastN = N;
		char* fileBuf = NULL;
		int length = 0;

		// receive data package with chunk and put chunk in fileBuf
		if (!receiveDataPackage(al->fd, &N, &fileBuf, &length)) {
			printf("ERROR: Could not receive data package.\n");
			free(fileBuf);
			return 0;
		}

		if (N != 0 && lastN + 1 != N) {
			printf("ERROR: Received sequence no. was %d instead of %d.\n", N,
					lastN + 1);
			free(fileBuf);
			return 0;
		}

		// write received chunk to output file
		fwrite(fileBuf, sizeof(char), length, outputFile);
		free(fileBuf);

		// increment no. of read bytes
		fileSizeReadSoFar += length;

		printProgressBar(fileSizeReadSoFar, fileSize);
	}
	printf("\n\n");

	if (DEBUG_MODE)
		printf("*** File chunks transfer complete. ***\n");

	// close output file
	if (fclose(outputFile) != 0) {
		printf("ERROR: Closing output file.\n");
		return -1;
	}

	// receive end control package
	int controlPackageTypeReceived = -1;
	if (!receiveControlPackage(al->fd, &controlPackageTypeReceived, 0, NULL)) {
		printf("ERROR: Could not receive END control package.\n");
		return 0;
	}

	if (controlPackageTypeReceived != CTRL_PKG_END) {
		printf("ERROR: Control field received (%d) is not END.\n",
				controlPackageTypeReceived);
		return 0;
	}

	if (!llclose(al->fd, ll->mode)) {
		printf("ERROR: Serial port was not closed.\n");
		return 0;
	}

	printf("\n");
	printf("File successfully received.\n");

	return 1;
}

int sendControlPackage(int fd, int C, char* fileSize, char* fileName) {
	if (DEBUG_MODE) {
		if (C == CTRL_PKG_START)
			printf("Sending START control package.\n");
		else if (C == CTRL_PKG_END)
			printf("Sending END control package.\n");
		else
			printf("WARNING: Sending UNKNOWN control package (C = %d).\n", C);
	}

	// calculate control package size
	int packageSize = 5 + strlen(fileSize) + strlen(fileName);
	ui i = 0, pos = 0;

	// create control package
	unsigned char controlPackage[packageSize];
	controlPackage[pos++] = C;
	controlPackage[pos++] = PARAM_FILE_SIZE;
	controlPackage[pos++] = strlen(fileSize);
	for (i = 0; i < strlen(fileSize); i++)
		controlPackage[pos++] = fileSize[i];
	controlPackage[pos++] = PARAM_FILE_NAME;
	controlPackage[pos++] = strlen(fileName);
	for (i = 0; i < strlen(fileName); i++)
		controlPackage[pos++] = fileName[i];

	if (C == CTRL_PKG_START) {
		printf("\n");
		printf("File: %s\n", fileName);
		printf("Size: %s (bytes)\n", fileSize);
		printf("\n");
	}

	// send control package
	if (!llwrite(fd, controlPackage, packageSize)) {
		printf(
				"ERROR: Could not write to link layer while sending control package.\n");
		free(controlPackage);

		return 0;
	}

	if (DEBUG_MODE) {
		if (C == CTRL_PKG_START)
			printf("START control package sent.\n");
		else if (C == CTRL_PKG_END)
			printf("END control package sent.\n");
		else
			printf("WARNING: UNKNOWN control package sent (C = %d).\n", C);
	}

	return 1;
}

int receiveControlPackage(int fd, int* controlPackageType, int* fileLength,
		char** fileName) {
	// receive control package
	unsigned char* package;
	ui totalSize = llread(fd, &package);
	if (totalSize < 0) {
		printf(
				"ERROR: Could not read from link layer while receiving control package.\n");
		return 0;
	}

	// process control package type
	*controlPackageType = package[0];

	if (*controlPackageType == CTRL_PKG_END) {
		if (DEBUG_MODE)
			printf("END control package has been received.\n");
		return 1;
	} else if (DEBUG_MODE)
		printf("START control package has been received.\n");

	ui i = 0, numParams = 2, pos = 1, numOcts = 0;
	for (i = 0; i < numParams; i++) {
		int paramType = package[pos++];

		switch (paramType) {
		case PARAM_FILE_SIZE: {
			numOcts = (ui) package[pos++];

			char* length = malloc(numOcts);
			memcpy(length, &package[pos], numOcts);

			*fileLength = atoi(length);
			free(length);

			break;
		}
		case PARAM_FILE_NAME:
			numOcts = (unsigned char) package[pos++];
			memcpy(*fileName, &package[pos], numOcts);

			break;
		}
	}

	return 1;
}

int sendDataPackage(int fd, int N, const char* buffer, int length) {
	unsigned char C = CTRL_PKG_DATA;
	unsigned char L2 = length / 256;
	unsigned char L1 = length % 256;

	// calculate package size
	ui packageSize = 4 + length;

	// allocate space for package header and file chunk
	unsigned char* package = (unsigned char*) malloc(packageSize);

	// build package header
	package[0] = C;
	package[1] = N;
	package[2] = L2;
	package[3] = L1;

	// copy file chunk to package
	memcpy(&package[4], buffer, length);

	// write package
	if (!llwrite(fd, package, packageSize)) {
		printf(
				"ERROR: Could not write to link layer while sending data package.\n");
		free(package);

		return 0;
	}

	free(package);

	return 1;
}

int receiveDataPackage(int fd, int* N, char** buf, int* length) {
	unsigned char* package;

	// read package from link layer
	ui size = llread(fd, &package);
	if (size < 0) {
		printf(
				"ERROR: Could not read from link layer while receiving data package.\n");
		return 0;
	}

	int C = package[0];
	*N = (unsigned char) package[1];
	int L2 = package[2];
	int L1 = package[3];

	// assert package is a data package
	if (C != CTRL_PKG_DATA) {
		printf("ERROR: Received package is not a data package (C = %d).\n", C);
		return 0;
	}

	// calculate size of the file chunk contained in the read package
	*length = 256 * L2 + L1;

	// allocate space for that file chunk
	*buf = malloc(*length);

	// copy file chunk to the buffer
	memcpy(*buf, &package[4], *length);

	// destroy the received package
	free(package);

	return 1;
}
