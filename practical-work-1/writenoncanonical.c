/*
* Non-Canonical Input Processing
*
* Authors:
*  Henrique Ferrolho
*  Joao Pereira
*  Miguel Ribeiro
*
*/
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define MODEMDEVICE "/dev/ttyS4"
#define FALSE 0
#define TRUE !FALSE
#define BAUDRATE B38400
#define FLAG 0x7E
#define A 0x03
#define C 0x03

typedef enum {
	START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP
} State;

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

void saveCurrentPortSettingsAndSetNewTermios(int fd, struct termios* oldtio, struct termios* newtio) {
	saveCurrentPortSettings(fd, oldtio);
	setNewTermios(fd, newtio);
}

void printBuf(unsigned char* buf) {
	printf("-----------------\n");
	printf("- FLAG: %x\t-\n", buf[0]);
	printf("- A: %x\t\t-\n", buf[1]);
	printf("- C: %x\t\t-\n", buf[2]);
	printf("- BCC: %x = %x\t-\n", buf[3], buf[1] ^ buf[2]);
	printf("- FLAG: %x\t-\n", buf[4]);
	printf("-----------------\n");
}

void cleanBuf(unsigned char* buf, unsigned int bufSize) {
	memset(buf, 0, bufSize * sizeof(*buf));
}

void createSETBuf(unsigned char* buf, unsigned int bufSize) {
	cleanBuf(buf, bufSize);

	buf[0] = FLAG;
	buf[1] = A;
	buf[2] = C;
	buf[3] = buf[1] ^ buf[2];
	buf[4] = FLAG;
}

void sendSET(int fd, unsigned char* buf, unsigned int bufSize) {
	createSETBuf(buf, bufSize);

	printf("Sending SET... ");
	
	write(fd, buf, bufSize * sizeof(*buf));

	printf("OK!\n");
}


// returns 1 on success
int receiveUA(int fd, unsigned char* buf, unsigned int size) {
	printf("Waiting for UA... ");

	int numReadBytes;
	State state = START;
	int debugStateMachine = 0;
	
	volatile int done = FALSE;
	while (!done) {
		unsigned char c;

		if (state != STOP) {
			numReadBytes = read(fd, &c, 1);

			if (!numReadBytes)
				return 0;
		}

		switch (state) {
		case START:
			if (c == FLAG) {
				if (debugStateMachine)
					printf("START: FLAG received. Going to FLAG_RCV.\n");
				buf[START] = c;
				state = FLAG_RCV;
			}
			break;
		case FLAG_RCV:
			if (c == A) {
				if (debugStateMachine)
					printf("FLAG_RCV: A received. Going to A_RCV.\n");
				buf[FLAG_RCV] = c;
				state = A_RCV;
			} else if (c != FLAG)
				state = START;
			break;
		case A_RCV:
			if (c == C) {
				if (debugStateMachine)
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
				if (debugStateMachine)
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
				if (debugStateMachine)
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

int main(int argc, char** argv) {
	if (argc < 2 || strcmp(MODEMDEVICE, argv[1]) != 0) {
		printf("Usage:\tnserial SerialPort\n\tex: nserial %s\n", MODEMDEVICE);
		exit(1);
	}

	int fd = openSerialPort(argv[1]);

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