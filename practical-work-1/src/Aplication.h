#pragma once

#include <termios.h>
#include "ConnectionMode.h"

#define MAX_SIZE 256

struct applicationLayer {
	int fileDescriptor; /*Descritor correspondente à porta série*/
	int status; /*TRANSMITTER | RECEIVER*/
	char * fileName ;
};

int sendCtrlPackage(int fd, int C,char* fileSize,char* fileName);

int sendDataPackage(int fd, int seq_number, const char* buffer, int length);

int receiveDataPackege(int fd, int* seq_number, char** buffer, int* length);

int receiveCtrlPackage(int fd, int* ctrl, int* fileLength, char** fileName);

int sendFile(const char* port, const char* file_name);

int getFileSize(FILE* file);

char * toArray(int number);

int sendFile(const char* port, const char* file_name);

int receiveFile(const char* port, const char* file_name);