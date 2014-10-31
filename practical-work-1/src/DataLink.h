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

typedef enum {
	INPUT_OUTPUT_ERROR, BCC1_ERROR, BCC2_ERROR
} MessageError;

typedef enum {
	COMMAND_SIZE = 5 * sizeof(char), MESSAGE_SIZE = 6 * sizeof(char)
} MessageSize;

typedef struct {
	MessageType type;

	int ns, nr;

	Command command;

	struct {
		unsigned char* message;
		ui messageSize;
	} data;

	MessageError error;
} Message;

typedef struct {
	// port /dev/ttySx
	char port[20];

	// connection mode
	ConnnectionMode mode;

	// transmission speed
	int baudRate;

	// frame sequence number (0, 1)
	ui ns;

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
int llwrite(int fd, const unsigned char* buf, ui bufSize);
int llread(int fd, unsigned char** message);
int llclose(int fd, ConnnectionMode mode);

unsigned char* createCommand(ControlField C);
int sendCommand(int fd, Command command);
Command getCommandWithControlField(ControlField controlField);
ControlField getCommandControlField(char* commandStr, Command command);

unsigned char* createMessage(const unsigned char* buf, ui bufSize);
int sendMessage(int fd, const unsigned char* buf, ui bufSize);

Message* receiveMessage(int fd);
int messageIsCommand(Message* msg, Command command);

ui stuff(unsigned char** buf, ui bufSize);
ui destuff(unsigned char** buf, ui bufSize);

unsigned char processBCC(const unsigned char* buf, ui size);

void cleanBuf(unsigned char* buf, ui bufSize);
void printBuf(unsigned char* buf);
