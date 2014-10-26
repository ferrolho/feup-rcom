#pragma once

#include <termios.h>
#include "ConnectionMode.h"

#define MAX_SIZE 256

struct applicationLayer {
	int fileDescriptor; /*Descritor correspondente à porta série*/
	int status; /*TRANSMITTER | RECEIVER*/
	char * fileName ;
};

int SendCtrlPackage(int fd, int ValC,char* fileSizeOct,char* fileName);

int SendDataPackage(int fd, int ValC,char* fileSizeOct,char* fileName);

int receiveDataPackage(int fd, int* seq_number, char** buffer, int* length);

int receiveCtrlPackage(int fd, int* ctrl, int* fileLength, char* fileName);

