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

extern int DEBUG_MODE;
extern ApplicationLayer* al;

int initApplicationLayer(const char* port, ConnnectionMode mode, int baudrate,
		int messageDataMaxSize, int numRetries, int timeout, char* file);
void printConnectionInfo();
int startConnection();
void printConnectionStatistics();

int sendFile();
int receiveFile();

int sendControlPackage(int fd, int C, char* fileSize, char* fileName);
int receiveControlPackage(int fd, int* ctrl, int* fileLength, char** fileName);

int sendDataPackage(int fd, int N, const char* buf, int length);
int receiveDataPackage(int fd, int* N, char** buf, int* length);
