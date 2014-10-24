#pragma once

#include <termios.h>
#include "ConnectionMode.h"

int init(char* port, ConnnectionMode mode);
int openSerialPort(char* serialPort);

void saveCurrentPortSettingsAndSetNewTermios(ConnnectionMode mode, int fd,
		struct termios* oldtio, struct termios* newtio);
void saveCurrentPortSettings(int fd, struct termios* oldtio);
void setNewTermios(ConnnectionMode mode, int fd, struct termios* newtio);

int llopen(ConnnectionMode mode, int fd, unsigned char* buf,
		unsigned int bufSize);
int send(int fd, unsigned char* buf, unsigned int bufSize);
int receive(int fd, unsigned char* buf, unsigned int bufSize);

void createSETBuf(unsigned char* buf, unsigned int bufSize);
void cleanBuf(unsigned char* buf, unsigned int bufSize);
void printBuf(unsigned char* buf);
