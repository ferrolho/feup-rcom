#pragma once

#include <termios.h>
#include "ConnectionMode.h"

int dataLink(const char* port, ConnnectionMode mode);
int openSerialPort(const char* port);

void saveCurrentPortSettingsAndSetNewTermios(ConnnectionMode mode, int fd,
		struct termios* oldtio, struct termios* newtio);
void saveCurrentPortSettings(int fd, struct termios* oldtio);
void setNewTermios(ConnnectionMode mode, int fd, struct termios* newtio);

int llopen(const char* port, ConnnectionMode mode);
int llclose(int fd, ConnnectionMode mode);
int send(int fd, unsigned char* buf, unsigned int bufSize);
int receive(int fd, unsigned char* buf, unsigned int bufSize);

void createSETBuf(unsigned char* buf, unsigned int bufSize);
void cleanBuf(unsigned char* buf, unsigned int bufSize);
void printBuf(unsigned char* buf);
