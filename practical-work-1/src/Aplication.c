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
#include "ConnectionMode.h"
#include "Utilities.h"

#define FILE_SIZE 0
#define FILE_NAME 1

int C_PKG_START = 2;
int C_PKG_END = 3;

ApplicationLayer* al;

int initApplicationLayer(const char* port, ConnnectionMode mode, char* file) {
	al = (ApplicationLayer*) malloc(sizeof(ApplicationLayer));

	al->fd = openSerialPort(port);
	if (al->fd < 0) {
		printf("ERROR: Serial port could not be open.\n");
		return 0;
	}
	al->mode = mode;
	al->fileName = file;

	if (!initLinkLayer(port, mode)) {
		printf("ERROR: Could not initialize Link Layer.\n");
		return 0;
	}

	startConnection();

	if (!closeSerialPort(al->fd))
		return 0;

	return 1;
}

int startConnection() {
	switch (al->mode) {
	case RECEIVE:
		printf("Starting connection in RECEIVE mode.\n");
		receiveFile();
		break;
	case SEND:
		printf("Starting connection in SEND mode.\n");
		sendFile();
		break;
	default:
		break;
	}

	return 1;
}

int sendFile() {
	FILE* file = fopen(al->fileName, "rb");
	if (!file) {
		printf("ERROR: Could not open file to be sent.\n");
		return 0;
	}

	int fd = llopen(ll->mode);
	if (fd <= 0)
		return 0;

	int fileSize = getFileSize(file);
	if (fileSize == -1) {
		printf("ERROR: Could not get size of file to be sent.\n");
		return 0;
	}

	char fileSizeBuf[sizeof(int) * 3 + 2];
	snprintf(fileSizeBuf, sizeof fileSizeBuf, "%d", fileSize);

	if (!sendControlPackage(al->fd, C_PKG_START, fileSizeBuf, al->fileName)) {
		printf("ERROR: Control package could not be sent.\n");
		return 0;
	}

	printf("Start control package sent.\n");

	char* buf = malloc(MAX_SIZE);
	ui readBytes = 0, writtenBytes = 0;

	int i = 0;
	while ((readBytes = fread(buf, sizeof(char), MAX_SIZE, file)) != 0) {
		printf("Reading file chunk.\n");
		if (!sendDataPackage(fd, (i++) % 255, buf, readBytes)) {
			printf("ERROR: Data package could not be sent.\n");
			free(buf);
			return 0;
		}

		writtenBytes += readBytes;

		buf = memset(buf, 0, MAX_SIZE);

		printf("\rProgress: %3d%%",
				(int) (writtenBytes / (float) fileSize * 100));
		fflush(stdout);
	}
	printf("\n");

	free(buf);

	if (fclose(file) != 0) {
		printf("ERROR: Unable to close file.\n");
		return 0;
	}

	if (!sendControlPackage(fd, C_PKG_END, "0", "")) {
		printf("ERROR: Could not sent Control package.\n");
		return 0;
	}

	if (!llclose(al->fd, ll->mode)) {
		printf("ERROR: Serial port was not closed.\n");
		return 0;
	}

	return 1;
}

int receiveFile() {
	int fd = llopen(ll->mode);
	if (fd <= 0)
		return 0;

	int controlStart, fileSize;
	char* fileName;

	if (!receiveControlPackage(al->fd, &controlStart, &fileSize, &fileName)) {
		printf("ERROR: Could not receive START control package.\n");
		return 0;
	}

	if (controlStart != C_PKG_START) {
		printf(
				"ERROR: Control package received but its control field - %d - is not C_PKG_START",
				controlStart);
		return 0;
	}

	FILE* outputFile = fopen(al->fileName, "wb");
	if (outputFile == NULL) {
		printf("ERROR: Could not create output file.\n");
		return 0;
	}

	int fileSizeReadSoFar = 0;
	int sn = -1;

	while (fileSizeReadSoFar != fileSize) {
		char* buf = (char*) calloc('0', sizeof(char));

		int length = 0;
		int lastSn = sn;

		if (!receiveDataPackage(al->fd, &sn, &buf, &length)) {
			printf("ERROR: Could not receive data package.\n");
			free(buf);
			return 0;
		}

		if (sn != 0 && sn != 1 && lastSn != !sn) {
			printf("ERROR: Received sequence no. was %d instead of %d.\n", sn,
					!lastSn);
			free(buf);
			return 0;
		}

		fileSizeReadSoFar += length;
		fwrite(buf, sizeof(char), length, outputFile);
		free(buf);
	}

	if (fclose(outputFile) != 0) {
		printf("ERROR: Closing output file.\n");
		return -1;
	}

	int controlEnd;
	if (!receiveControlPackage(al->fd, &controlEnd, (int*) 0, (char**) "")) {
		printf("ERROR: Received control package end.\n");
		return 0;
	}

	if (controlEnd != C_PKG_END) {
		printf("ERROR: Control field received (%d) is not END.\n", controlEnd);
		return 0;
	}

	if (!llclose(al->fd, ll->mode)) {
		printf("ERROR: Serial port was not closed.\n");
		return 0;
	}

	return 1;
}

int sendControlPackage(int fd, int C, char* fileSize, char* fileName) {
	int bufSize = 1 + strlen(fileSize) + strlen(fileName);
	char controlPackage[bufSize];

	controlPackage[0] = C;

	// file size
	controlPackage[1] = 0;
	controlPackage[2] = strlen(fileSize);

	ui i = 0, pos = 3;
	for (i = 0; i < strlen(fileSize); i++)
		controlPackage[pos++] = fileSize[i];

	// file name
	controlPackage[pos++] = 1;
	controlPackage[pos++] = strlen(fileName);

	for (i = 0; i < strlen(fileName); i++)
		controlPackage[pos++] = fileName[i];

	printf("File: %s\n", fileName);
	printf("Size: %s (bytes)\n", fileSize);

	if (C == 2)
		printf("Sending START control package.\n");
	if (C == 3)
		printf("Sending END control package.\n");

	if (!llwrite(fd, &controlPackage[0], bufSize)) {
		printf(
				"ERROR: Could not write to link layer while sending control package.\n");
		free(controlPackage);

		return 0;
	}

	return 1;
}

int receiveControlPackage(int fd, int* package, int* fileLength,
		char** fileName) {
	char* receivedPackage;
	ui totalSize = llread(fd, &receivedPackage);
	if (totalSize < 0) {
		printf(
				"ERROR: Could not read from link layer while receiving control package.\n");
		return 0;
	}

	printf("Received a control package.\n");
	*package = receivedPackage[0];

	ui numParams = 2, pos = 1, numOcts = 0;

	unsigned int i;
	for (i = 0; i < numParams; i++) {
		int paramType = receivedPackage[pos++];

		switch (paramType) {
		case FILE_SIZE: {
			numOcts = (ui) receivedPackage[pos++];

			char* length = malloc(numOcts);
			memcpy(length, &receivedPackage[pos], numOcts);

			*fileLength = atoi(length);
			free(length);

			break;
		}
		case FILE_NAME:
			numOcts = (unsigned char) receivedPackage[pos++];
			memcpy(*fileName, &receivedPackage[pos], numOcts);

			break;
		}
	}

	return 1;
}

int sendDataPackage(int fd, int sn, const char* buffer, int length) {
	int C = 1;
	int L2 = length / 256;
	int L1 = length % 256;

	ui packageSize = 4 + (L2 + L1);

	char* controlPackage = (char*) malloc(packageSize);

	controlPackage[0] = C;
	controlPackage[1] = sn;
	controlPackage[2] = L2;
	controlPackage[3] = L1;
	memcpy(&controlPackage[4], buffer, length);

	if (!llwrite(fd, controlPackage, packageSize)) {
		printf(
				"ERROR: Could not write to link layer while sending data package.\n");
		free(controlPackage);

		return 0;
	}

	free(controlPackage);

	return 1;
}

int receiveDataPackage(int fd, int* sn, char** buf, int* length) {
	char* receivedPackage;
	ui size = llread(fd, &receivedPackage);
	if (size < 0) {
		printf(
				"ERROR: Could not read from link layer while receiving data package.\n");
		return 0;
	}

	if (receivedPackage[0] != 1) {
		printf("ERROR: Received package is not a data package (C = %d).\n",
				receivedPackage[0]);
		return 0;
	}

	int seqNumber = (unsigned char) receivedPackage[1];

	int bufSize = 256 * (unsigned char) receivedPackage[2]
			+ (unsigned char) receivedPackage[3];

	*buf = malloc(bufSize);
	memcpy(*buf, &receivedPackage[4], bufSize);

	free(receivedPackage);

	*sn = seqNumber;
	*length = bufSize;

	return 1;
}
