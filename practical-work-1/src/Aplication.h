#pragma once

#include <termios.h>
#include "ConnectionMode.h"

#define MAX_SIZE 256

struct applicationLayer {
	int fileDescriptor; /*Descritor correspondente à porta série*/
	int status; /*TRANSMITTER | RECEIVER*/
	char * fileName ;
};

int sendCtrlPackage(int fd, int ValC,char* fileSize,char* fileName);

int sendDataPackage(int fd, int ValC,char* fileSize,char* fileName);

int receiveDataPackage(int fd, int* seq_number, char** buffer, int* length);

int receiveCtrlPackage(int fd, int* ctrl, int* fileLength, char* fileName);

int sendFile(const char* port, const char* file_name);

char * toArray(int number);

int getFileSize(FILE* file);

