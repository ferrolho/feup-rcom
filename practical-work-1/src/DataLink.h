#pragma once

#include <termios.h>
#include "ConnectionMode.h"

#define MAX_SIZE 256

typedef enum {
	START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP
} State;

typedef enum {
	SET, UA, RR, REJ, DISC
} Command;

typedef enum {
	C_SET = 0x03, C_UA = 0x07, C_RR = 0x05, C_REJ = 0x01, C_DISC = 0x0B
} ControlField;

typedef struct {
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
} LinkLayer;

int dataLink(const char* port, ConnnectionMode mode);

int openSerialPort(const char* port);
int closeSerialPort(int fd, struct termios* oldtio);

int saveCurrentPortSettingsAndSetNewTermios(ConnnectionMode mode, int fd,
		struct termios* oldtio, struct termios* newtio);
int saveCurrentPortSettings(int fd, struct termios* oldtio);
int setNewTermios(ConnnectionMode mode, int fd, struct termios* newtio);

int llopen(const char* port, ConnnectionMode mode);
int llwrite();
int llread();
int llclose(int fd, ConnnectionMode mode);

int send(int fd, unsigned char* buf, unsigned int bufSize);
int receive(int fd, unsigned char* buf, unsigned int bufSize);

void createCommand(ControlField C, unsigned char* buf, unsigned int bufSize);
void cleanBuf(unsigned char* buf, unsigned int bufSize);
void printBuf(unsigned char* buf);
void printDISC(unsigned char* buf);
