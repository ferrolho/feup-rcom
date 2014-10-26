#pragma once

#include <termios.h>
#include "ConnectionMode.h"

#define MAX_SIZE 256

struct applicationLayer {
	int fileDescriptor; /*Descritor correspondente à porta série*/
	int status; /*TRANSMITTER | RECEIVER*/
	char * fileName ;
};

int SendCntrlPackage(int ValC,char* fileSizeOct,char* fileName);

int SendDataPackage(int ValC,char* fileSizeOct,char* fileName);
