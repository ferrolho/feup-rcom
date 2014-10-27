#pragma once

#include <termios.h>
#include "ConnectionMode.h"
#include "Utilities.h"

typedef enum {
	START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP
} State;

typedef enum {
	SET, UA, RR, REJ, DISC
} Command;

typedef enum {
	C_SET = 0x03, C_UA = 0x07, C_RR = 0x05, C_REJ = 0x01, C_DISC = 0x0B
} ControlField;

typedef enum {
	COMMAND, DATA, INVALID
} MessageType;

typedef struct {
	MessageType type;
	Command command;
	unsigned char* message;
	ui messageSize;
} Message;

typedef struct {
	// port /dev/ttySx
	char port[20];

	// connection mode
	ConnnectionMode mode;

	// transmission speed
	int baudRate;

	// frame sequence number (0, 1)
	ui sequenceNumber;

	// timeout value
	ui timeout;

	// number of retries in case of failure
	ui numTries;

	// trama
	char frame[MAX_SIZE];

	// old and new termios
	struct termios oldtio, newtio;
} LinkLayer;

extern LinkLayer* ll;

int initLinkLayer(const char* port, ConnnectionMode mode);

int saveCurrentPortSettingsAndSetNewTermios();
int saveCurrentTermiosSettings();
int setNewTermios();

int openSerialPort(const char* port);
int closeSerialPort();

int llopen(ConnnectionMode mode);
int llwrite(int fd, const char* buf, ui bufSize);
int llread(int fd, char** message);
int llclose(int fd, ConnnectionMode mode);

void createCommand(ControlField C, unsigned char* buf, ui bufSize);
int sendCommand(int fd, Command command);
Command getCommandWithControlField(ControlField controlField);
ControlField getCommandControlField(char* commandStr, Command command);

char* createMessage(const char* buf, ui bufSize, int sn);
int sendMessage(int fd, const char* buf, ui bufSize);

Message* receive(int fd);
int messageIsCommand(Message* msg, Command command);

ui stuff(char** buf, ui bufSize);
ui destuff(char** buf, ui bufSize);

void cleanBuf(unsigned char* buf, ui bufSize);
void printBuf(unsigned char* buf);
