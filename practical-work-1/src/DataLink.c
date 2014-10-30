#include "DataLink.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "Alarm.h"
#include "Aplication.h"

const int BAUDRATE = B38400;
const int FLAG = 0x7E;
const int A = 0x03;
const int ESCAPE = 0x7D;

LinkLayer* ll;

// TODO change this
int numRetries = 3;
#define MAX_FRAME_SIZE 256

int initLinkLayer(const char* port, ConnnectionMode mode) {
	ll = (LinkLayer*) malloc(sizeof(LinkLayer));

	strcpy(ll->port, port);
	ll->mode = mode;
	ll->baudRate = BAUDRATE;
	ll->ns = 0;
	ll->timeout = 3;
	ll->numTries = 1 + numRetries;

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

			if (messageIsCommand(receiveMessage(al->fd), UA)) {
				connected = 1;

				printf("*** Successfully established a connection. ***\n");
			}
		}

		stopAlarm();

		break;
	}
	case RECEIVE: {
		while (!connected) {
			if (messageIsCommand(receiveMessage(al->fd), SET)) {
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

		Message* receivedMessage = receiveMessage(fd);

		if (messageIsCommand(receivedMessage, RR)) {
			if (ll->ns != receivedMessage->nr)
				ll->ns = receivedMessage->nr;

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
		msg = receiveMessage(fd);

		switch (msg->type) {
		case INVALID:
			printf("INVALID message received.\n");

			//ll->ns = msg->ns;
			//sendCommand(fd, REJ);

			break;
		case COMMAND:
			if (msg->command == DISC)
				done = 1;
			break;
		case DATA:
			if (ll->ns == msg->ns) {
				*message = malloc(msg->messageSize);
				memcpy(*message, msg->message, msg->messageSize);
				free(msg->message);

				ll->ns = !msg->ns;
				sendCommand(fd, RR);

				done = 1;
			} else
				printf("\tBad Nr associated, ignoring message.\n");

			break;
		}
	}

	stopAlarm();

	return 1;
}

int llclose(int fd, ConnnectionMode mode) {
	printf("*** Terminating connection. ***\n");

	int try = 0, disconnected = 0;

	switch (mode) {
	case SEND: {
		while (!disconnected) {
			if (try == 0 || alarmWentOff) {
				alarmWentOff = FALSE;

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

			if (messageIsCommand(receiveMessage(fd), DISC))
				disconnected = TRUE;
		}

		stopAlarm();
		sendCommand(fd, UA);
		sleep(1);

		printf("*** Connection terminated. ***\n");

		return 1;
	}
	case RECEIVE: {
		while (!disconnected) {
			if (messageIsCommand(receiveMessage(fd), DISC))
				disconnected = TRUE;
		}

		int uaReceived = FALSE;
		while (!uaReceived) {
			if (try == 0 || alarmWentOff) {
				alarmWentOff = FALSE;

				if (try >= ll->numTries) {
					stopAlarm();
					printf("ERROR: Disconnect could not be sent.\n");
					return 0;
				}

				sendCommand(fd, DISC);

				if (++try == 1)
					setAlarm();
			}

			if (messageIsCommand(receiveMessage(fd), UA))
				uaReceived = TRUE;
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

char* createCommand(ControlField C) {
	char* command = malloc(COMMAND_SIZE);

	command[0] = FLAG;
	command[1] = A;
	command[2] = C;
	if (C == C_REJ || C == C_RR)
		command[2] |= (ll->ns << 7);
	command[3] = command[1] ^ command[2];
	command[4] = FLAG;

	return command;
}

int sendCommand(int fd, Command command) {
	char commandStr[MAX_SIZE];
	ControlField ctrlField = getCommandControlField(commandStr, command);

	char* commandBuf = createCommand(ctrlField);
	ui commandBufSize = stuff(&commandBuf, COMMAND_SIZE);

	int successfullySentCommand = TRUE;
	if (write(fd, commandBuf, commandBufSize) != COMMAND_SIZE) {
		printf("ERROR: Could not write %s command.\n", commandStr);
		successfullySentCommand = FALSE;
	}

	free(commandBuf);

	return successfullySentCommand;
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

char* createMessage(const char* buf, ui bufSize) {
	char* message = malloc(MESSAGE_SIZE + bufSize);

	char C = ll->ns << 6;

	char BCC1 = A ^ C;

	char BCC2 = 0;
	int i;
	for (i = 0; i < bufSize; i++)
		BCC2 ^= buf[i];

	message[0] = FLAG;
	message[1] = A;
	message[2] = C;
	message[3] = BCC1;
	memcpy(&message[4], buf, bufSize);
	message[4 + bufSize] = BCC2;
	message[5 + bufSize] = FLAG;

	return message;
}

int sendMessage(int fd, const char* message, ui messageSize) {
	char* msg = createMessage(message, messageSize);
	messageSize += MESSAGE_SIZE;

	messageSize = stuff(&msg, messageSize);

	ui numWrittenBytes = write(fd, msg, messageSize);
	if (numWrittenBytes != messageSize)
		perror("ERROR: error while sending message.\n");

	free(msg);

	return numWrittenBytes == messageSize;
}

const int DEBUG_STATE_MACHINE = TRUE;

Message* receiveMessage(int fd) {
	Message* msg = (Message*) malloc(sizeof(Message));
	msg->type = INVALID;

	State state = START;

	ui bufSize = 0;
	char* buf = malloc(MAX_FRAME_SIZE);

	volatile int done = FALSE;
	while (!done) {
		unsigned char c;

		// if not stopping
		if (state != STOP) {
			// read message
			int numReadBytes = read(fd, &c, 1);

			// if nothing was read
			if (!numReadBytes) {
				printf("ERROR: nothing received.\n");

				// return invalid message
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

				// if writing at the end and more bytes will still be received
				if (bufSize % MAX_FRAME_SIZE == 0) {
					int m = bufSize / MAX_FRAME_SIZE + 1;
					buf = (char*) realloc(buf, m * MAX_FRAME_SIZE);
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

	bufSize = destuff(&buf, bufSize);

	char A = buf[1];
	char C = buf[2];
	char BCC1 = buf[3];

	if ((A ^ C) != BCC1) {
		printf("ERROR: invalid BCC1.\n");
		msg->type = INVALID;
		free(buf);
		return msg;
	}

	if (msg->type == COMMAND) {
		// get message command
		msg->command = getCommandWithControlField(buf[2]);

		// get command control field
		char commandStr[MAX_SIZE];
		ControlField controlField = getCommandControlField(commandStr,
				msg->command);

		if (msg->command == RR || msg->command == REJ)
			msg->nr = (controlField >> 7) & BIT(1);
	} else if (msg->type == DATA) {
		msg->messageSize = bufSize - MESSAGE_SIZE;

		// calculate bcc2
		char calculatedBCC2 = 0;
		int i;
		for (i = 4; i < msg->messageSize; i++)
			calculatedBCC2 ^= buf[i];

		// get received bcc2
		char BCC2 = buf[4 + msg->messageSize];

		if (calculatedBCC2 != BCC2) {
			printf("ERROR: invalid BCC2: 0x%02x != 0x%02x.\n", calculatedBCC2,
					BCC2);
			msg->type = INVALID;
			free(buf);
			return msg;
		}

		msg->ns = (buf[2] >> 6) & BIT(1);

		// copy message
		msg->message = malloc(msg->messageSize);
		memcpy(msg->message, &buf[4], msg->messageSize);
	}

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
