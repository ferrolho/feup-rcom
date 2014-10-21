#pragma once

#include <termios.h>

int openSerialPort(char* serialPort);
void saveCurrentPortSettings(int fd, struct termios* oldtio);
void setNewTermios(int fd, struct termios* newtio);
void saveCurrentPortSettingsAndSetNewTermios(int fd, struct termios* oldtio, struct termios* newtio);
void printBuf(unsigned char* buf);
void cleanBuf(unsigned char* buf, unsigned int bufSize);
void createSETBuf(unsigned char* buf, unsigned int bufSize);
void sendSET(int fd, unsigned char* buf, unsigned int bufSize);
int receiveUA(int fd, unsigned char* buf, unsigned int size);
void sendSETAndReceiveUA(int fd, unsigned char* buf, unsigned int bufSize);
int sender(char* port);
