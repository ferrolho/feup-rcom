#pragma once

#include "stdio.h"
#include <termios.h>
#include "ConnectionMode.h"

struct applicationLayer {
	int fileDescriptor; /*Descritor correspondente à porta série*/
	int status; /*TRANSMITTER | RECEIVER*/
	char* fileName;
};

int sendCtrlPackage(int fd, int C, char* fileSize, char* fileName);
int sendDataPackage(int fd, int seqNumber, const char* buffer, int length);
int receiveDataPackege(int fd, int* seqNumber, char** buffer, int* length);
int receiveCtrlPackage(int fd, int* ctrl, int* fileLength, char** fileName);
int sendFile(const char* port, const char* fileName);
int getFileSize(FILE* file);
char* toArray(int number);
int sendFile(const char* port, const char* fileName);
int receiveFile(const char* port, const char* fileName);
