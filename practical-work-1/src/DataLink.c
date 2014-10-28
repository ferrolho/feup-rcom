#include "DataLink.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "Aplication.h"

const int FALSE = 0;
const int TRUE = 1;

const int BAUDRATE = B38400;
const int FLAG = 0x7E;
const int A = 0x03;
const int ESCAPE = 0x7D;

LinkLayer* ll;

int initLinkLayer(const char* port, ConnnectionMode mode) {
	ll = (LinkLayer*) malloc(sizeof(LinkLayer));

	strcpy(ll->port, port);
	ll->mode = mode;
	ll->baudRate = BAUDRATE;
	ll->sequenceNumber = 0;
	ll->timeout = 3;
	ll->numTries = 4;

	if (!saveCurrentPortSettingsAndSetNewTermios()) {
		printf(
				"ERROR: Could not save current port settings and set new termios.\n");
		return 0;
	}

	return 1;
}

int saveCurrentPortSettingsAndSetNewTermios() {
	if (!saveCurrentTermiosSettings()) {
		printf("ERROR: Could not save current termios settings.\n");
		return 0;
	}

	if (!setNewTermios()) {
		printf("ERROR: Could not set new termios settings.\n");
		return 0;
	}

	return 1;
}

int saveCurrentTermiosSettings() {
	if (tcgetattr(al->fd, &ll->oldtio) != 0) {
		printf("ERROR: Could not save current termios settings.\n");
		return 0;
	}

	return 1;
}

int setNewTermios() {
	bzero(&ll->newtio, sizeof(ll->newtio));
	ll->newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	ll->newtio.c_iflag = IGNPAR;
	ll->newtio.c_oflag = 0;
	ll->newtio.c_lflag = 0;

	// inter-character timer unused
	ll->newtio.c_cc[VTIME] = 3;

	// blocking read until x chars received
	ll->newtio.c_cc[VMIN] = 0;

	if (tcflush(al->fd, TCIOFLUSH) != 0)
		return 0;

	if (tcsetattr(al->fd, TCSANOW, &ll->newtio) != 0)
		return 0;

	printf("New termios structure set.\n");

	return 1;
}

int openSerialPort(const char* port) {
	// Open serial port device for reading and writing and not as controlling
	// tty because we don't want to get killed if linenoise sends CTRL-C.
	return open(port, O_RDWR | O_NOCTTY);
}

int closeSerialPort() {
	if (tcsetattr(al->fd, TCSANOW, &ll->oldtio) == -1) {
		perror("tcsetattr");
		return 0;
	}

	close(al->fd);

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

int llopen(ConnnectionMode mode) {
	printf("*** Trying to establish a connection. ***\n");

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

				sendCommand(al->fd, SET);

				if (++try == 1)
					setAlarm();
			}

			if (messageIsCommand(receive(al->fd), UA)) {
				connected = 1;

				printf("*** Successfully established a connection. ***\n");
			}
		}

		stopAlarm();

		break;
	}
	case RECEIVE: {
		while (!connected) {
			if (messageIsCommand(receive(al->fd), SET)) {
				sendCommand(al->fd, UA);
				connected = 1;

				printf("*** Successfully established a connection. ***\n");
			}
		}

		break;
	}
	default:
		break;
	}

	return al->fd;
}

int llwrite(int fd, const char* buf, ui bufSize) {
	int try = 0, transferring = 1;

	while (transferring) {
		if (try == 0 || alarmWentOff) {
			alarmWentOff = 0;

			if (try >= ll->numTries) {
				stopAlarm();
				printf("ERROR: Maximum number of retries exceeded.\n");
				printf("Message not sent.\n");
				return 0;
			}

			sendMessage(fd, buf, bufSize);

			if (++try == 1)
				setAlarm();
		}

		Message* receivedMessage = receive(fd);

		if (messageIsCommand(receivedMessage, RR)) {
			stopAlarm();
			transferring = 0;
		} else if (messageIsCommand(receivedMessage, REJ)) {
			stopAlarm();
			try = 0;
		}
	}

	stopAlarm();

	return 1;
}

int llread(int fd, char** message) {
	Message* msg = NULL;

	int done = 0;
	while (!done) {
		msg = receive(fd);

		switch (msg->type) {
		case INVALID:
			sendCommand(fd, C_REJ);
			break;
		case COMMAND:
			if (msg->command == DISC)
				done = 1;
			break;
		case DATA:
			*message = malloc(msg->messageSize);
			memcpy(*message, msg->message, msg->messageSize);
			free(msg->message);

			ll->sequenceNumber = !ll->sequenceNumber;
			sendCommand(fd, RR);

			done = 1;

			break;
		}
	}

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

				sendCommand(fd, DISC);

				if (++try == 1)
					setAlarm();
			}

			if (messageIsCommand(receive(fd), DISC))
				disconnected = 1;
		}

		stopAlarm();
		sendCommand(fd, UA);
		sleep(1);

		printf("*** Connection terminated. ***\n");

		return 1;
	}
	case RECEIVE: {
		while (!disconnected) {
			if (messageIsCommand(receive(fd), DISC))
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

				sendCommand(fd, DISC);

				if (++try == 1)
					setAlarm();
			}

			if (messageIsCommand(receive(fd), UA))
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

void createCommand(ControlField C, unsigned char* buf, ui bufSize) {
	cleanBuf(buf, bufSize);

	buf[0] = FLAG;
	buf[1] = A;
	buf[2] = C;
	buf[3] = buf[1] ^ buf[2];
	buf[4] = FLAG;
}

int sendCommand(int fd, Command command) {
	char commandStr[MAX_SIZE];
	ControlField ctrlField = getCommandControlField(commandStr, command);

	ui bufSize = 5;
	unsigned char buf[bufSize];
	createCommand(ctrlField, buf, bufSize);

	if (write(fd, buf, bufSize * sizeof(*buf))) {
		printf("SENT: %s\n", commandStr);
		// printBuf(buf);

		return 1;
	} else
		printf("ERROR: Could not write %s command.\n", commandStr);

	return 0;
}

Command getCommandWithControlField(ControlField controlField) {
	switch (controlField) {
	case C_SET:
		return SET;
	case C_UA:
		return UA;
	case C_RR:
		return RR;
	case C_REJ:
		return REJ;
	case C_DISC:
		return DISC;
	default:
		return SET;
	}
}

ControlField getCommandControlField(char* commandStr, Command command) {
	switch (command) {
	case SET:
		strcpy(commandStr, "SET");
		return C_SET;
	case UA:
		strcpy(commandStr, "UA");
		return C_UA;
	case RR:
		strcpy(commandStr, "RR");
		return C_RR;
	case REJ:
		strcpy(commandStr, "REJ");
		return C_REJ;
	case DISC:
		strcpy(commandStr, "DISC");
		return C_DISC;
	default:
		strcpy(commandStr, "* ERROR *");
		return C_SET;
	}
}

const int DEBUG_STATE_MACHINE = 0;

const int MSG_SIZE = 6 * sizeof(char);

char* createMessage(const char* buf, ui bufSize, int sn) {
	char* msg = malloc(MSG_SIZE + bufSize);

	msg[0] = FLAG;
	msg[1] = A;
	msg[2] = sn << 1;
	msg[3] = msg[1] ^ msg[2];

	memcpy(&msg[4], buf, bufSize);

	char BCC = 0;
	int i;
	for (i = 0; i < bufSize; i++)
		BCC ^= buf[i];

	msg[4 + bufSize] = BCC;
	msg[5 + bufSize] = FLAG;

	return msg;
}

int sendMessage(int fd, const char* buf, ui bufSize) {
	char* msg = createMessage(buf, bufSize, ll->sequenceNumber);
	bufSize += MSG_SIZE;

	bufSize = stuff(&msg, bufSize);

	ui numWrittenBytes = write(fd, msg, bufSize);
	if (numWrittenBytes != bufSize)
		perror("ERROR: error while sending message.\n");

	free(msg);

	return numWrittenBytes == bufSize;
}

#define FRAME_SIZE 256

Message* receive(int fd) {
	Message* msg = (Message*) malloc(sizeof(Message));
	msg->type = INVALID;

	ui bufSize = 0;
	unsigned char* buf = malloc(FRAME_SIZE);

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
				msg->type = INVALID;
				return msg;
			}
		}

		switch (state) {
		case START:
			if (c == FLAG) {
				if (DEBUG_STATE_MACHINE)
					printf("START: FLAG received. Going to FLAG_RCV.\n");
				buf[bufSize++] = c;
				state = FLAG_RCV;
			}
			break;
		case FLAG_RCV:
			if (c == A) {
				if (DEBUG_STATE_MACHINE)
					printf("FLAG_RCV: A received. Going to A_RCV.\n");
				buf[bufSize++] = c;
				state = A_RCV;
			} else if (c != FLAG) {
				bufSize = 0;
				state = START;
			}
			break;
		case A_RCV:
			if (c != FLAG) {
				if (DEBUG_STATE_MACHINE)
					printf("A_RCV: C received. Going to C_RCV.\n");
				buf[bufSize++] = c;
				state = C_RCV;
			} else if (c == FLAG)
				state = FLAG_RCV;
			else {
				bufSize = 0;
				state = START;
			}
			break;
		case C_RCV:
			if (c == (A ^ buf[A_RCV])) {
				if (DEBUG_STATE_MACHINE)
					printf("C_RCV: BCC received. Going to BCC_OK.\n");
				buf[bufSize++] = c;
				state = BCC_OK;
			} else if (c == FLAG)
				state = FLAG_RCV;
			else {
				bufSize = 0;
				state = START;
			}
			break;
		case BCC_OK:
			if (c == FLAG) {
				if (msg->type == INVALID) {
					msg->type = COMMAND;

					buf[bufSize++] = c;
				} else {
					char BCC = 0;
					int i;
					for (i = 0; i < bufSize; i++)
						BCC ^= buf[i];

					buf[bufSize++] = BCC;

					buf[bufSize++] = c;
				}

				state = STOP;

				if (DEBUG_STATE_MACHINE)
					printf("BCC_OK: FLAG received. Going to STOP.\n");
			} else if (c != FLAG) {
				if (msg->type == COMMAND) {
					state = START;
					continue;
				} else if (msg->type == INVALID)
					msg->type = DATA;

				if (bufSize % FRAME_SIZE == 0) {
					int m = bufSize / FRAME_SIZE + 1;
					buf = (unsigned char*) realloc(buf, m * FRAME_SIZE);
				}

				buf[bufSize++] = c;
			}
			break;
		case STOP:
			buf[bufSize] = 0;
			done = TRUE;
			break;
		default:
			break;
		}
	}

	if (msg->type == COMMAND) {
		msg->command = getCommandWithControlField(buf[2]);

		char commandStr[MAX_SIZE];
		getCommandControlField(commandStr, msg->command);
		printf("RECEIVED: %s\n", commandStr);
	} else if (msg->type == DATA) {
		msg->messageSize = bufSize;
		msg->message = malloc(msg->messageSize);
		memcpy(msg->message, &buf[4], msg->messageSize);
	}

	// printBuf(buf);
	free(buf);

	return msg;
}

int messageIsCommand(Message* msg, Command command) {
	return msg->type == COMMAND && msg->command == command;
}

ui stuff(char** buf, ui bufSize) {
	ui newBufSize = bufSize;

	int i;
	for (i = 1; i < bufSize - 1; i++)
		if ((*buf)[i] == FLAG || (*buf)[i] == ESCAPE)
			newBufSize++;

	*buf = (char*) realloc(*buf, newBufSize);

	for (i = 1; i < bufSize - 1; i++) {
		if ((*buf)[i] == FLAG || (*buf)[i] == ESCAPE) {
			memmove(*buf + i + 1, *buf + i, bufSize - i);

			bufSize++;

			(*buf)[i] = ESCAPE;
			(*buf)[i + 1] ^= 0x20;
		}
	}

	return newBufSize;
}

ui destuff(char** buf, ui bufSize) {
	int i;
	for (i = 1; i < bufSize - 1; ++i) {
		if ((*buf)[i] == ESCAPE) {
			memmove(*buf + i, *buf + i + 1, bufSize - i - 1);

			bufSize--;

			(*buf)[i] ^= 0x20;
		}
	}

	*buf = (char*) realloc(*buf, bufSize);

	return bufSize;
}

void cleanBuf(unsigned char* buf, ui bufSize) {
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
