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

#define MAX_PACKET_SIZE 255
#define FILE_SIZE 0
#define FILE_NAME 1

int C_START = 2;
int C_END = 3;

int sendCtrlPackage(int fd, int C, char* fileSize, char* fileName) {
	if (C == 2)
		printf("\nControl Package START\n");
	if (C == 3)
		printf("\n\nControl Package END\n");

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

	/*-----------------------------------------------------------*/

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
		perror("ll_write");
		free(controlPackage);
		return -1;
	}

	return 0;
}

int sendDataPackage(int fd, int sn, const char* buffer, int length) {

	int C = 1;
	int l2 = length / 256;
	int l1 = length % 256;

	ui packetSize = 4 + (l2 + l1); // 1 + 1 + 1 + 1 + (l2 + l1)

	char* ctrlPackage = (char*) malloc(packetSize);

	ctrlPackage[0] = C;
	ctrlPackage[1] = sn;
	ctrlPackage[2] = l2;
	ctrlPackage[3] = l1;
	memcpy(&ctrlPackage[4], buffer, length);

	if (!llwrite(fd, ctrlPackage, packetSize)) {
		perror("ll_write");
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
		perror("ll_read");
		return 0;
	}

	if (receivedPackage[0] != 1) {
		printf("Received packet is NOT a Data Package C = %d",
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

	return 0;
}

int receiveCtrlPackage(int fd, int* ctrl, int* fileLength, char** fileName) {

	char* receivedPackage;
	ui totalSize = llread(fd, &receivedPackage);
	if (totalSize < 0) {
		perror("ll_read");
		return 0;
	}

	*ctrl = receivedPackage[0];

	ui numParams = 2, current_index = 1, numOcts, i;

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

	return 0;
}

int getFileSize(FILE* file) {
	int file_size;
	if (fseek(file, 0L, SEEK_END) == -1) {
		perror("fseek");
		return -1;
	}

	file_size = ftell(file);

	return file_size;
}


int sendFile(char* port, char* fileName) {
	FILE* file = fopen(fileName, "r");
	if (!file) {
		perror("fopen");
		return -1;
	}

	int fd = llopen(port, SEND);
	if (fd == -1) {
		perror("llopen");
		return -1;
	}

	int file_size = getFileSize(file);
	if (file_size == -1) {
		perror("getFileSize");
		return -1;
	}

	char file_size_buf[sizeof(int)*3+2];
	snprintf(file_size_buf, sizeof file_size_buf, "%d", file_size);

	if (sendCtrlPackage(fd, C_START, file_size_buf, fileName) != 0) {
		perror("sendCtrlPackage");
		return -1;
	}

	int i = 0;
	char* buffer = malloc(MAX_PACKET_SIZE);
	ui readBytes, writeBytes = 0;

	while ((readBytes = fread(buffer, sizeof(char), MAX_PACKET_SIZE, file)) != 0) {
		if (sendDataPackage(fd, (i++) % 255, buffer, readBytes) == -1) {
			perror("sendDataPackage");
			free(buffer);
			return -1;
		}

		writeBytes += readBytes;

		buffer = memset(buffer, 0, MAX_PACKET_SIZE);

		printf("\rProgress: %3d%%",
				(int) (writeBytes / (float) file_size * 100));
		fflush(stdout);
	}
	printf("\n");

	free(buffer);

	if (fclose(file) != 0) {
		perror("fclose");
		return -1;
	}

	if (sendCtrlPackage(fd, C_END, "0", "") != 0) {
		perror("sendCtrlPackage");
		return -1;
	}

	if (!llclose(fd, SEND)) { // TODO check if mode is SEND
		perror("llclose");
		return -1;
	}

	return 0;
}

int receiveFile(char* port, char* file_name) {
	int fd = llopen(port, RECEIVE);
	if (fd == -1) {
		perror("llopen");
		return -1;
	}

	int ctrl_start, fileSize;
	char * fileName;
	if (receiveCtrlPackage(fd, &ctrl_start, &fileSize, &fileName) != 0) {
		perror("receiveCtrlPackage (START)");
		return -1;
	}

	if (ctrl_start != C_START) {
		printf("Control field received (%d) is not START", ctrl_start);
		return -1;
	}

	FILE* output_file = fopen(fileName, "wb");
	if (output_file == NULL) {
		perror("ERROR: Opening output file");
		return -1;
	}

	int total_size_read = 0;
	int seq_number = -1;
	while (total_size_read != fileSize) {

		char* buf = (char*) calloc('0', sizeof(char));

		int length = 0;
		int seq_number_before = seq_number;
		if (receiveDataPackage(fd, &seq_number, &buf, &length) != 0) {
			perror("receiveDataPackage");
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
		fwrite(buf, 1, length, output_file);
		free(buf);
	}

	if (fclose(output_file) != 0) {
		perror("ERROR: Closing output file");
		return -1;
	}

	int ctrl_end;
	if (receiveCtrlPackage(fd, &ctrl_end, (int *)0, (char **)"") != 0) {
		perror("app_receive_control_packet (end)");
		return -1;
	}

	if (ctrl_end != C_END) {
		printf("Control field received (%d) is not END", ctrl_end);
		return -1;
	}

	if (!llclose(fd, RECEIVE)) {
		perror("ll_close");
		return -1;
	}

	return 0;
}
