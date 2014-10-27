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

#define FILE_SIZE 0
#define FILE_NAME 1

int C_START = 2;
int C_END = 3;

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
		printf("ERROR: fopen\n");
		return -1;
	}

	int fd = llopen(ll->mode);
	if (fd <= 0)
		return 0;

	int file_size = getFileSize(file);
	if (file_size == -1) {
		perror("getFileSize");
		return -1;
	}

	char file_size_buf[sizeof(int) * 3 + 2];
	snprintf(file_size_buf, sizeof file_size_buf, "%d", file_size);

	if (sendCtrlPackage(al->fd, C_START, file_size_buf, al->fileName) != 0) {
		printf("ERROR: sendCtrlPackage\n");
		return -1;
	}

	int i = 0;
	char* buffer = malloc(MAX_SIZE);
	ui readBytes = 0, writeBytes = 0;

	while ((readBytes = fread(buffer, sizeof(char), MAX_SIZE, file)) != 0) {
		if (sendDataPackage(fd, (i++) % 255, buffer, readBytes) == -1) {
			printf("ERROR: sendDataPackage\n");
			free(buffer);
			return -1;
		}

		writeBytes += readBytes;

		buffer = memset(buffer, 0, MAX_SIZE);

		printf("\rProgress: %3d%%",
				(int) (writeBytes / (float) file_size * 100));
		fflush(stdout);
	}
	printf("\n");

	free(buffer);

	if (fclose(file) != 0) {
		printf("fclose\n");
		return -1;
	}

	if (sendCtrlPackage(fd, C_END, "0", "") != 0) {
		printf("sendCtrlPackage\n");
		return -1;
	}

	if (!llclose(al->fd, ll->mode)) {
		printf("ERROR: Serial port was not closed.\n");
		return 0;
	}

	return 0;
}

int receiveFile() {
	int fd = llopen(ll->mode);
	if (fd <= 0)
		return 0;

	int ctrlStart, fileSize;
	char* fileName;

	if (!receiveCtrlPackage(al->fd, &ctrlStart, &fileSize, &fileName)) {
		printf("Receive START control package.\n");
		return -1;
	}

	if (ctrlStart != C_START) {
		printf("Control field received (%d) is not START", ctrlStart);
		return -1;
	}

	FILE* outputFile = fopen(al->fileName, "wb");
	if (outputFile == NULL) {
		printf("ERROR: Could not create output file.\n");
		return -1;
	}

	int total_size_read = 0;
	int seq_number = -1;
	while (total_size_read != fileSize) {

		char* buf = (char*) calloc('0', sizeof(char));

		int length = 0;
		int seq_number_before = seq_number;
		if (!receiveDataPackage(al->fd, &seq_number, &buf, &length)) {
			printf("ERROR: receiveDataPackage\n");
			free(buf);
			return -1;
		}

		if (seq_number != 0 && seq_number_before + 1 != seq_number) {
			printf("Expected sequence number %d but got %d",
					seq_number_before + 1, seq_number);
			free(buf);
			return -1;
		}

		total_size_read += length;
		fwrite(buf, 1, length, outputFile);
		free(buf);
	}

	if (fclose(outputFile) != 0) {
		printf("ERROR: Closing output file.\n");
		return -1;
	}

	int ctrlEnd;
	if (!receiveCtrlPackage(al->fd, &ctrlEnd, (int*) 0, (char**) "")) {
		printf("ERROR: Received control package end.\n");
		return -1;
	}

	if (ctrlEnd != C_END) {
		printf("ERROR: Control field received (%d) is not END.\n", ctrlEnd);
		return -1;
	}

	if (!llclose(al->fd, ll->mode)) {
		printf("ERROR: Serial port was not closed.\n");
		return 0;
	}

	return 0;
}

int sendCtrlPackage(int fd, int C, char* fileSize, char* fileName) {
	if (C == 2)
		printf("Control Package START\n");
	if (C == 3)
		printf("Control Package END\n");

	int totalBufferSize = 1 + strlen(fileSize) + strlen(fileName);

	char controlPackage[totalBufferSize];
	controlPackage[0] = C; //start = 1 end = 2

	//tamanho do ficheiro
	controlPackage[1] = 0;
	controlPackage[2] = strlen(fileSize);

	int position_act = 3;

	unsigned int i = 0;
	for (i = 0; i < strlen(fileSize); i++) {
		controlPackage[position_act] = fileSize[i];
		position_act++;
	}

	// nome do ficheiro
	controlPackage[position_act] = 1;
	position_act++;
	controlPackage[position_act] = strlen(fileName);
	position_act++;

	i = 0;
	for (i = 0; i < strlen(fileName); i++) {
		controlPackage[position_act] = fileName[i];
		position_act++;
	}

	printf("Nome: %s  Tamanho do ficheiro em bytes: %s\n", fileName, fileSize);

	if (!llwrite(fd, &controlPackage[0], totalBufferSize)) {
		printf("llwrite\n");
		free(controlPackage);
		return -1;
	}

	return 0;
}

int sendDataPackage(int fd, int sn, const char* buffer, int length) {
	int C = 1;
	int l2 = length / 256;
	int l1 = length % 256;

	ui packageSize = 4 + (l2 + l1); // 1 + 1 + 1 + 1 + (l2 + l1)

	char* ctrlPackage = (char*) malloc(packageSize);

	ctrlPackage[0] = C;
	ctrlPackage[1] = sn;
	ctrlPackage[2] = l2;
	ctrlPackage[3] = l1;
	memcpy(&ctrlPackage[4], buffer, length);

	if (!llwrite(fd, ctrlPackage, packageSize)) {
		printf("llwrite\n");
		free(ctrlPackage);
		return -1;
	}

	free(ctrlPackage);
	return 0;
}

int receiveDataPackage(int fd, int* sn, char** buf, int* length) {
	char* receivedPackage;
	ui totalSize = llread(fd, &receivedPackage);
	if (totalSize < 0) {
		perror("llread");
		return 0;
	}

	if (receivedPackage[0] != 1) {
		printf("ERROR: Received package is NOT a Data Package C = %d",
				receivedPackage[0]);
		return 0;
	}

	int seq = (unsigned char) receivedPackage[1];

	int bufSize = 256 * (unsigned char) receivedPackage[2]
			+ (unsigned char) receivedPackage[3];

	*buf = malloc(bufSize);
	memcpy(*buf, &receivedPackage[4], bufSize);

	free(receivedPackage);

	*sn = seq;
	*length = bufSize;

	return 1;
}

int receiveCtrlPackage(int fd, int* ctrl, int* fileLength, char** fileName) {
	char* receivedPackage;
	ui totalSize = llread(fd, &receivedPackage);
	if (totalSize < 0) {
		printf("ERROR: Received package is empty.\n");
		return 0;
	}

	printf("Received package.\n");
	*ctrl = receivedPackage[0];

	ui numParams = 2, current_index = 1;
	unsigned int numOcts, i;

	for (i = 0; i < numParams; i++) {

		int paramType = receivedPackage[current_index];
		current_index++;

		switch (paramType) {
		case FILE_SIZE: {
			numOcts = (ui) receivedPackage[current_index];
			current_index++;

			char* length = malloc(numOcts);
			memcpy(length, &receivedPackage[current_index], numOcts);

			*fileLength = atoi(length);

			free(length);
			break;
		}

		case FILE_NAME:
			numOcts = (unsigned char) receivedPackage[current_index];
			current_index++;

			memcpy(*fileName, &receivedPackage[current_index], numOcts);

			break;
		}
	}

	return 1;
}

int getFileSize(FILE* file) {
	int file_size;
	if (fseek(file, 0L, SEEK_END) == -1) {
		printf("fseek\n");
		return -1;
	}

	file_size = ftell(file);

	return file_size;
}
