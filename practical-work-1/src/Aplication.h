#pragma once

#include <stdio.h>
#include <termios.h>
#include "ConnectionMode.h"

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

int sendControlPackage(int fd, int C, char* fileSize, char* fileName);
int receiveControlPackage(int fd, int* ctrl, int* fileLength, char** fileName);

int sendDataPackage(int fd, int sn, const char* buf, int length);
int receiveDataPackage(int fd, int* sn, char** buf, int* length);
