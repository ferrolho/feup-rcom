#include "DataLink.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const int FALSE = 0;
const int TRUE = 1;

const int BAUDRATE = B38400;
const int FLAG = 0x7E;
const int A = 0x03;

LinkLayer* ll;

int initLinkLayer(const char* port, ConnnectionMode mode) {
	ll = (LinkLayer*) malloc(sizeof(LinkLayer));

	strcpy(ll->port, port);
	ll->mode = mode;
	ll->baudRate = BAUDRATE;
	ll->timeout = 3;
	ll->numTries = 4;

	startDataLink();

	return 1;
}

int startDataLink() {
	int fd = openSerialPort(ll->port);
	if (fd < 0)
		return 0;

	struct termios oldtio, newtio;
	if (!saveCurrentPortSettingsAndSetNewTermios(ll->mode, fd, &oldtio,
			&newtio))
		return 0;

	int portfd = llopen(ll->port, ll->mode);
	if (portfd <= 0)
		return 0;

	if (!llclose(portfd, ll->mode))
		return 0;

	if (!closeSerialPort(fd, &oldtio))
		return 0;

	return 1;
}

int openSerialPort(const char* port) {
	// Open serial port device for reading and writing and not as controlling
	// tty because we don't want to get killed if linenoise sends CTRL-C.
	return open(port, O_RDWR | O_NOCTTY);
}

int closeSerialPort(int fd, struct termios* oldtio) {
	if (tcsetattr(fd, TCSANOW, oldtio) == -1) {
		perror("tcsetattr");
		return 0;
	}

	close(fd);

	return 1;
}

int saveCurrentPortSettingsAndSetNewTermios(ConnnectionMode mode, int fd,
		struct termios* oldtio, struct termios* newtio) {
	if (!saveCurrentPortSettings(fd, oldtio))
		return 0;

	if (!setNewTermios(mode, fd, newtio))
		return 0;

	return 1;
}

int saveCurrentPortSettings(int fd, struct termios* oldtio) {
	if (tcgetattr(fd, oldtio) != 0)
		return 0;

	return 1;
}

int setNewTermios(ConnnectionMode mode, int fd, struct termios* newtio) {
	bzero(newtio, sizeof(*newtio));
	newtio->c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	newtio->c_iflag = IGNPAR;
	newtio->c_oflag = 0;
	newtio->c_lflag = 0;

	// inter-character timer unused
	newtio->c_cc[VTIME] = 3;

	// blocking read until x chars received
	newtio->c_cc[VMIN] = 0;

	if (tcflush(fd, TCIOFLUSH) != 0)
		return 0;

	if (tcsetattr(fd, TCSANOW, newtio) != 0)
		return 0;

	printf("New termios structure set.\n");

	return 1;
}

int alarmWentOff;

void alarmHandler(int signal) {
	if (signal != SIGALRM)
		return;

	alarmWentOff = 1;
	printf("Connection time out!\n\nRetrying:\n");

	alarm(ll->timeout);
}

void setAlarm() {
	signal(SIGALRM, alarmHandler);

	alarmWentOff = 0;

	alarm(ll->timeout);
}

void stopAlarm() {
	alarm(0);
}

int llopen(const char* port, ConnnectionMode mode) {
	printf("*** Trying to establish a connection. ***\n");

	int fd = openSerialPort(port);
	if (fd < 0)
		return fd;

	int try = 0, connected = 0;

	switch (mode) {
	case SEND: {
		while (!connected) {
			if (try == 0 || alarmWentOff) {
				alarmWentOff = 0;

				if (try >= ll->numTries) {
					stopAlarm();
					printf("ERROR: Maximum number of retries exceeded.\n");
					printf("*** Connection aborted. ***\n");
					return 0;
				}

				send(fd, SET);

				if (++try == 1)
					setAlarm();
			}

			if (receive(fd, UA)) {
				connected = 1;

				printf("*** Successfully established a connection. ***\n");
			}
		}

		stopAlarm();

		break;
	}
	case RECEIVE: {
		while (!connected) {
			if (receive(fd, SET)) {
				send(fd, UA);
				connected = 1;

				printf("*** Successfully established a connection. ***\n");
			}
		}

		break;
	}
	default:
		break;
	}

	return fd;
}

int llwrite() {
	return 1;
}

int llread() {
	return 1;
}

int llclose(int fd, ConnnectionMode mode) {
	printf("*** Terminating connection. ***\n");

	int try = 0, disconnected = 0;

	switch (mode) {
	case SEND: {
		while (!disconnected) {
			if (try == 0 || alarmWentOff) {
				alarmWentOff = 0;

				if (try >= ll->numTries) {
					stopAlarm();
					printf("ERROR: Maximum number of retries exceeded.\n");
					printf("*** Connection aborted. ***\n");
					return 0;
				}

				send(fd, DISC);

				if (++try == 1)
					setAlarm();
			}

			if (receive(fd, DISC))
				disconnected = 1;
		}

		stopAlarm();
		send(fd, UA);
		sleep(1);

		printf("*** Connection terminated. ***\n");

		return 1;
	}
	case RECEIVE: {
		while (!disconnected) {
			if (receive(fd, DISC))
				disconnected = 1;
		}

		int uaReceived = 0;
		while (!uaReceived) {
			if (try == 0 || alarmWentOff) {
				alarmWentOff = 0;

				if (try >= ll->numTries) {
					stopAlarm();
					printf("ERROR: Disconnect could not be sent.\n");
					return -1;
				}

				send(fd, DISC);

				if (++try == 1)
					setAlarm();
			}

			if (receive(fd, UA))
				uaReceived = 1;
		}

		stopAlarm();
		printf("*** Connection terminated. ***\n");

		return 1;
	}
	default:
		break;
	}

	return 0;
}

int send(int fd, Command command) {
	// TODO refactor this - pull up method
	char commandStr[MAX_SIZE];
	ControlField ctrlField;

	switch (command) {
	case SET:
		strcpy(commandStr, "SET");
		ctrlField = C_SET;
		break;
	case UA:
		strcpy(commandStr, "UA");
		ctrlField = C_UA;
		break;
	case RR:
		strcpy(commandStr, "RR");
		ctrlField = C_RR;
		break;
	case REJ:
		strcpy(commandStr, "REJ");
		ctrlField = C_REJ;
		break;
	case DISC:
		strcpy(commandStr, "DISC");
		ctrlField = C_DISC;
		break;
	default:
		return 0;
	}

	unsigned int bufSize = 5;
	unsigned char buf[bufSize];
	createCommand(ctrlField, buf, bufSize);

	if (write(fd, buf, bufSize * sizeof(*buf))) {
		printf("SENT: %s\n", commandStr);
		printBuf(buf);

		return 1;
	} else
		printf("ERROR: Could not write %s command.\n", commandStr);

	return 0;
}

const int DEBUG_STATE_MACHINE = 0;

int receive(int fd, Command command) {
	unsigned int bufSize = 5;
	unsigned char buf[bufSize];
	// cleanBuf(buf, bufSize);

	State state = START;

	volatile int done = FALSE;
	while (!done) {
		unsigned char c;

		if (state != STOP) {
			int numReadBytes = read(fd, &c, 1);

			if (DEBUG_STATE_MACHINE) {
				printf("Number of bytes read: %d\n", numReadBytes);
				if (numReadBytes)
					printf("Read char: 0x%02x\n", c);
			}

			if (!numReadBytes) {
				// printf("ERROR: nothing received.\n");
				return 0;
			}
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
			if (c != FLAG) {
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
			if (c == (A ^ buf[A_RCV])) {
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

	// TODO refactor this - pull up method
	char commandStr[MAX_SIZE];
	ControlField ctrlField;

	switch (command) {
	case SET:
		strcpy(commandStr, "SET");
		ctrlField = C_SET;
		break;
	case UA:
		strcpy(commandStr, "UA");
		ctrlField = C_UA;
		break;
	case RR:
		strcpy(commandStr, "RR");
		ctrlField = C_RR;
		break;
	case REJ:
		strcpy(commandStr, "REJ");
		ctrlField = C_REJ;
		break;
	case DISC:
		strcpy(commandStr, "DISC");
		ctrlField = C_DISC;
		break;
	default:
		return 0;
	}

	if (buf[2] == ctrlField) {
		printf("RECEIVED: %s.\n", commandStr);
		printBuf(buf);

		return 1;
	} else
		printf("ERROR: %s command was not received.\n", commandStr);

	return 0;
}

void createCommand(ControlField C, unsigned char* buf, unsigned int bufSize) {
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
