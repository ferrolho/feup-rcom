/*
* FEUP - RCOM
*
* Authors:
*  Henrique Ferrolho
*  Joao Pereira
*  Miguel Ribeiro
*
*/
#include "dataLink.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FALSE 0
#define TRUE !FALSE
#define BAUDRATE B38400
#define FLAG 0x7E
#define A 0x03
#define C 0x03

typedef enum {
	START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP
} State;

int openSerialPort(char* serialPort);
void saveCurrentPortSettingsAndSetNewTermios(int fd, struct termios* oldtio, struct termios* newtio);
void saveCurrentPortSettings(int fd, struct termios* oldtio);
void setNewTermios(int fd, struct termios* newtio);
void sendSETAndReceiveUA(int fd, unsigned char* buf, unsigned int bufSize);
void sendSET(int fd, unsigned char* buf, unsigned int bufSize);
int receiveUA(int fd, unsigned char* buf, unsigned int size);
void createSETBuf(unsigned char* buf, unsigned int bufSize);
void cleanBuf(unsigned char* buf, unsigned int bufSize);
void printBuf(unsigned char* buf);

int sender(char* port) {
	int fd = openSerialPort(port);

	struct termios oldtio, newtio;
	saveCurrentPortSettingsAndSetNewTermios(fd, &oldtio, &newtio);

	unsigned int bufSize = 5;
	unsigned char buf[bufSize];
	sendSETAndReceiveUA(fd, buf, bufSize);

	if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}
	close(fd);

	return 0;
}

int receiver(char* port) {
	int fd = openSerialPort(port);

	struct termios oldtio, newtio;
	saveCurrentPortSettingsAndSetNewTermios(fd, &oldtio, &newtio);

	unsigned int bufSize = 5;
	unsigned char buf[bufSize];

	receiveSET(fd, buf, bufSize);
	sendUA(fd, buf, bufSize);

	if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}
	close(fd);

	return 0;
}

int openSerialPort(char* serialPort) {
	// Open serial port device for reading and writing and not as controlling
	// tty because we don't want to get killed if linenoise sends CTRL-C.
	int fd = open(serialPort, O_RDWR | O_NOCTTY);

	if (fd < 0) {
		perror(serialPort);
		exit(-1);
	}

	return fd;
}

void saveCurrentPortSettingsAndSetNewTermios(int fd, struct termios* oldtio, struct termios* newtio) {
	saveCurrentPortSettings(fd, oldtio);
	setNewTermios(fd, newtio);
}

void saveCurrentPortSettings(int fd, struct termios* oldtio) {
	if (tcgetattr(fd, oldtio) == -1) {
		perror("tcgetattr");
		exit(-1);
	}
}

void setNewTermios(int fd, struct termios* newtio) {
	bzero(newtio, sizeof(*newtio));
	newtio->c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	newtio->c_iflag = IGNPAR;
	newtio->c_oflag = 0;
	newtio->c_lflag = 0;

	// inter-character timer unused
	newtio->c_cc[VTIME] = 30;

	// blocking read until x chars received
	newtio->c_cc[VMIN] = 0;

	tcflush(fd, TCIOFLUSH);
	if (tcsetattr(fd, TCSANOW, newtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	} else printf("New termios structure set.\n");
}

void sendSETAndReceiveUA(int fd, unsigned char* buf, unsigned int bufSize) {
	int try, numTries = 4;

	for (try = 0; try < numTries; try++) {
		sendSET(fd, buf, bufSize);

		if (receiveUA(fd, buf, bufSize)) {
			printBuf(buf);
			break;
		} else {
			if (try == numTries - 1)
				printf("Connection aborted.\n");
			else
				printf("Time out!\n\nRetrying: ");
		}
	}
}

void sendSET(int fd, unsigned char* buf, unsigned int bufSize) {
	createSETBuf(buf, bufSize);

	printf("Sending SET... ");
	
	write(fd, buf, bufSize * sizeof(*buf));

	printf("OK!\n");
}

const int DEBUG_STATE_MACHINE = 0;

// returns 1 on success
int receiveUA(int fd, unsigned char* buf, unsigned int size) {
	printf("Waiting for UA... ");

	int numReadBytes;
	State state = START;
	
	volatile int done = FALSE;
	while (!done) {
		unsigned char c;

		if (state != STOP) {
			numReadBytes = read(fd, &c, 1);

			if (DEBUG_STATE_MACHINE) {
				printf("Number of bytes read: %d\n", numReadBytes);
				if (numReadBytes)
					printf("Read char: 0x%02x\n", c);
			}

			if (!numReadBytes)
				return 0;
		}

		switch (state) {
		case START:
			if (c == FLAG) {
				if (DEBUG_STATE_MACHINE)
					printf("START: FLAG received. Going to FLAG_RCV.\n");
				buf[START] = c;
				state = FLAG_RCV;
			}
			break;
		case FLAG_RCV:
			if (c == A) {
				if (DEBUG_STATE_MACHINE)
					printf("FLAG_RCV: A received. Going to A_RCV.\n");
				buf[FLAG_RCV] = c;
				state = A_RCV;
			} else if (c != FLAG)
				state = START;
			break;
		case A_RCV:
			if (c == C) {
				if (DEBUG_STATE_MACHINE)
					printf("A_RCV: C received. Going to C_RCV.\n");
				buf[A_RCV] = c;
				state = C_RCV;
			} else if (c == FLAG)
				state = FLAG_RCV;
			else
				state = START;
			break;
		case C_RCV:
			if (c == (A ^ C)) {
				if (DEBUG_STATE_MACHINE)
					printf("C_RCV: BCC received. Going to BCC_OK.\n");
				buf[C_RCV] = c;
				state = BCC_OK;
			} else if (c == FLAG)
				state = FLAG_RCV;
			else
				state = START;
			break;
		case BCC_OK:
			if (c == FLAG) {
				if (DEBUG_STATE_MACHINE)
					printf("BCC_OK: FLAG received. Going to STOP.\n");
				buf[BCC_OK] = c;
				state = STOP;
			} else
				state = START;
			break;
		case STOP:
			buf[STOP] = 0;
			done = TRUE;
			break;
		default:
			break;
		}
	}

	printf("OK!\n");

	return 1;
}

void receiveSET(int fd, unsigned char* buf, unsigned int size) {
	printf("Receiving SET... ");

	State state = START;
	
	volatile int done = FALSE;
	while (!done) {
		unsigned char c;

		if (state != STOP) {
			read(fd, &c, 1);
		}

		switch (state) {
		case START:
			if (c == FLAG) {
				buf[START] = c;
				state = FLAG_RCV;
			}
			break;
		case FLAG_RCV:
			if (c == A) {
				buf[FLAG_RCV] = c;
				state = A_RCV;
			} else if (c != FLAG)
				state = START;
			break;
		case A_RCV:
			if (c == C) {
				buf[A_RCV] = c;
				state = C_RCV;
			} else if (c == FLAG)
				state = FLAG_RCV;
			else
				state = START;
			break;
		case C_RCV:
			if (c == (A ^ C)) {
				buf[C_RCV] = c;
				state = BCC_OK;
			} else if (c == FLAG)
				state = FLAG_RCV;
			else
				state = START;
			break;
		case BCC_OK:
			if (c == FLAG) {
				buf[BCC_OK] = c;
				state = STOP;
			} else
				state = START;
			break;
		case STOP:
			buf[STOP] = 0;
			done = TRUE;
			break;
		default:
			break;
		}
	}

	printf("OK!\n");

	printBuf(buf);
}

void sendUA(int fd, unsigned char* buf, unsigned int size) {
	printf("Sending UA... ");

	write(fd, buf, size * sizeof(*buf));

	printf("OK!\n");
}

void createSETBuf(unsigned char* buf, unsigned int bufSize) {
	cleanBuf(buf, bufSize);

	buf[0] = FLAG;
	buf[1] = A;
	buf[2] = C;
	buf[3] = buf[1] ^ buf[2];
	buf[4] = FLAG;
}

void cleanBuf(unsigned char* buf, unsigned int bufSize) {
	memset(buf, 0, bufSize * sizeof(*buf));
}

void printBuf(unsigned char* buf) {
	printf("-------------------------\n");
	printf("- FLAG: 0x%02x\t\t-\n", buf[0]);
	printf("- A: 0x%02x\t\t-\n", buf[1]);
	printf("- C: 0x%02x\t\t-\n", buf[2]);
	printf("- BCC: 0x%02x = 0x%02x\t-\n", buf[3], buf[1] ^ buf[2]);
	printf("- FLAG: 0x%02x\t\t-\n", buf[4]);
	printf("-------------------------\n");
}
