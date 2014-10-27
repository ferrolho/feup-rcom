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
int sendDataPackage(int fd, int sn, const char* buffer, int length);
int receiveDataPackege(int fd, int* sn, char** buffer, int* length);
int receiveCtrlPackage(int fd, int* ctrl, int* fileLength, char** fileName);
int getFileSize(FILE* file);
char* toArray(int number);
int sendFile(char* port, char* fileName);
int receiveFile(char* port, char* fileName);
