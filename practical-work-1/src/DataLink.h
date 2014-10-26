#pragma once

#include <termios.h>
#include "ConnectionMode.h"

#define MAX_SIZE 256

struct LinkLayer {
	// port /dev/ttySx
	char port[20];

	// connection mode
	ConnnectionMode mode;

	// transmission speed
	int baudRate;

	// frame sequence number (0, 1)
	unsigned int sequenceNumber;

	// timeout value
	unsigned int timeout;

	// number of retries in case of failure
	unsigned int numTransmissions;

	// trama
	char frame[MAX_SIZE];
};

int dataLink(const char* port, ConnnectionMode mode);
int openSerialPort(const char* port);

void saveCurrentPortSettingsAndSetNewTermios(ConnnectionMode mode, int fd,
		struct termios* oldtio, struct termios* newtio);
void saveCurrentPortSettings(int fd, struct termios* oldtio);
void setNewTermios(ConnnectionMode mode, int fd, struct termios* newtio);

int llopen(const char* port, ConnnectionMode mode);
int llwrite();
int llread();
int llclose(int fd, ConnnectionMode mode);

int send(int fd, unsigned char* buf, unsigned int bufSize);
int receive(int fd, unsigned char* buf, unsigned int bufSize);
int receiveDISC(int fd, unsigned char* buf, unsigned int bufSize);

void createSETBuf(unsigned char* buf, unsigned int bufSize);
void cleanBuf(unsigned char* buf, unsigned int bufSize);
void printBuf(unsigned char* buf);
void printDISC(unsigned char* buf);
