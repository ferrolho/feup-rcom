#pragma once

#include <termios.h>

int sender(char* port);
int receiver(char* port);

int openSerialPort(char* serialPort);

void saveCurrentPortSettingsAndSetNewTermios(int fd, struct termios* oldtio, struct termios* newtio);
void saveCurrentPortSettings(int fd, struct termios* oldtio);
void setNewTermios(int fd, struct termios* newtio);

void sendSETAndReceiveUA(int fd, unsigned char* buf, unsigned int bufSize);
void send(int fd, unsigned char* buf, unsigned int bufSize);
int receive(int fd, unsigned char* buf, unsigned int bufSize);

void createSETBuf(unsigned char* buf, unsigned int bufSize);
void cleanBuf(unsigned char* buf, unsigned int bufSize);
void printBuf(unsigned char* buf);
