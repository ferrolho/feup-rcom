#pragma once

#include <stdio.h>
#include <termios.h>
#include "ConnectionMode.h"
#include "Utilities.h"

typedef struct {
	// serial port file descriptor
	int fd;

	// connection mode
	ConnnectionMode mode;

	// name of the file to be sent/received
	char* fileName;
} ApplicationLayer;

extern ApplicationLayer* al;

int initApplicationLayer(const char* port, ConnnectionMode mode, char* file);
int startConnection();

int sendFile();
int receiveFile();

int sendCtrlPackage(int fd, int C, char* fileSize, char* fileName);
int receiveCtrlPackage(int fd, int* ctrl, int* fileLength, char** fileName);

int sendDataPackage(int fd, int sn, const char* buffer, int length);
int receiveDataPackege(int fd, int* sn, char** buffer, int* length);

int getFileSize(FILE* file);
char* toArray(int number);
