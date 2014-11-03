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

const int FLAG = 0x7E;
const int A = 0x03;
const int ESCAPE = 0x7D;

LinkLayer* ll;

int getBaudrate(int baudrate) {
	switch (baudrate) {
	case 0:
		return B0;
	case 50:
		return B50;
	case 75:
		return B75;
	case 110:
		return B110;
	case 134:
		return B134;
	case 150:
		return B150;
	case 200:
		return B200;
	case 300:
		return B300;
	case 600:
		return B600;
	case 1200:
		return B1200;
	case 1800:
		return B1800;
	case 2400:
		return B2400;
	case 4800:
		return B4800;
	case 9600:
		return B9600;
	case 19200:
		return B19200;
	case 38400:
		return B38400;
	case 57600:
		return B57600;
	case 115200:
		return B115200;
	case 230400:
		return B230400;
	case 460800:
		return B460800;
	default:
		return -1;
	}
}

int initLinkLayer(const char* port, ConnnectionMode mode, int baudrate,
		int messageDataMaxSize, int numRetries, int timeout) {
	ll = (LinkLayer*) malloc(sizeof(LinkLayer));

	strcpy(ll->port, port);
	ll->mode = mode;
	ll->baudRate = baudrate;
	ll->messageDataMaxSize = messageDataMaxSize;
	ll->ns = 0;
	ll->timeout = timeout;
	ll->numTries = 1 + numRetries;
	ll->stats = initStatistics();

	if (!saveCurrentPortSettingsAndSetNewTermios()) {
		printf(
				"ERROR: Could not save current port settings and set new termios.\n");
		return 0;
	}

	return 1;
}

Statistics* initStatistics() {
	Statistics* stats = (Statistics*) malloc(sizeof(Statistics));

	stats->sentMessages = 0;
	stats->receivedMessages = 0;

	stats->timeouts = 0;

	stats->numSentRR = 0;
	stats->numReceivedRR = 0;

	stats->numSentREJ = 0;
	stats->numReceivedREJ = 0;

	return stats;
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
	ll->newtio.c_cflag = ll->baudRate | CS8 | CLOCAL | CREAD;
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

	if (DEBUG_MODE)
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

int llwrite(int fd, const unsigned char* buf, ui bufSize) {
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

int llread(int fd, unsigned char** message) {
	Message* msg = NULL;

	int done = 0;
	while (!done) {
		msg = receiveMessage(fd);

		switch (msg->type) {
		case INVALID:
			printf("INVALID message received.\n");

			if (msg->error == BCC2_ERROR) {
				ll->ns = msg->ns;
				sendCommand(fd, REJ);
			}

			break;
		case COMMAND:
			if (msg->command == DISC)
				done = 1;
			break;
		case DATA:
			if (ll->ns == msg->ns) {
				*message = malloc(msg->data.messageSize);
				memcpy(*message, msg->data.message, msg->data.messageSize);
				free(msg->data.message);

				ll->ns = !msg->ns;
				sendCommand(fd, RR);

				done = TRUE;

			} else
				printf("\tWrong message ns associated: ignoring message.\n");

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

unsigned char* createCommand(ControlField C) {
	unsigned char* command = malloc(COMMAND_SIZE);

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

	unsigned char* commandBuf = createCommand(ctrlField);
	ui commandBufSize = stuff(&commandBuf, COMMAND_SIZE);

	int successfullySentCommand = TRUE;
	if (write(fd, commandBuf, commandBufSize) != COMMAND_SIZE) {
		printf("ERROR: Could not write %s command.\n", commandStr);
		successfullySentCommand = FALSE;
	}

	free(commandBuf);

	if (command == REJ)
		ll->stats->numSentREJ++;
	else if (command == RR)
		ll->stats->numSentRR++;

	if (DEBUG_MODE)
		printf("Sent command: %s.\n", commandStr);

	return successfullySentCommand;
}

Command getCommandWithControlField(ControlField controlField) {
	switch (controlField & 0x0F) {
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
		printf("ERROR: control field not recognized.\n");
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

unsigned char* createMessage(const unsigned char* message, ui size) {
	unsigned char* msg = malloc(MESSAGE_SIZE + size);

	unsigned char C = ll->ns << 6;
	unsigned char BCC1 = A ^ C;
	unsigned char BCC2 = processBCC(message, size);

	msg[0] = FLAG;
	msg[1] = A;
	msg[2] = C;
	msg[3] = BCC1;
	memcpy(&msg[4], message, size);
	msg[4 + size] = BCC2;
	msg[5 + size] = FLAG;

	return msg;
}

int sendMessage(int fd, const unsigned char* message, ui messageSize) {
	unsigned char* msg = createMessage(message, messageSize);
	messageSize += MESSAGE_SIZE;

	messageSize = stuff(&msg, messageSize);

	ui numWrittenBytes = write(fd, msg, messageSize);
	if (numWrittenBytes != messageSize)
		perror("ERROR: error while sending message.\n");

	free(msg);

	ll->stats->sentMessages++;

	return numWrittenBytes == messageSize;
}

Message* receiveMessage(int fd) {
	Message* msg = (Message*) malloc(sizeof(Message));
	msg->type = INVALID;
	msg->ns = msg->nr = -1;

	State state = START;

	ui size = 0;
	unsigned char* message = malloc(ll->messageDataMaxSize);

	volatile int done = FALSE;
	while (!done) {
		unsigned char c;

		// if not stopping
		if (state != STOP) {
			// read message
			int numReadBytes = read(fd, &c, 1);

			// if nothing was read
			if (!numReadBytes) {
				if (DEBUG_MODE)
					printf("ERROR: nothing received.\n");

				free(message);

				msg->type = INVALID;
				msg->error = INPUT_OUTPUT_ERROR;

				return msg;
			}
		}

		switch (state) {
		case START:
			if (c == FLAG) {
				if (DEBUG_MODE)
					printf("START: FLAG received. Going to FLAG_RCV.\n");

				message[size++] = c;

				state = FLAG_RCV;
			}
			break;
		case FLAG_RCV:
			if (c == A) {
				if (DEBUG_MODE)
					printf("FLAG_RCV: A received. Going to A_RCV.\n");

				message[size++] = c;

				state = A_RCV;
			} else if (c != FLAG) {
				size = 0;

				state = START;
			}
			break;
		case A_RCV:
			if (c != FLAG) {
				if (DEBUG_MODE)
					printf("A_RCV: C received. Going to C_RCV.\n");

				message[size++] = c;

				state = C_RCV;
			} else if (c == FLAG) {
				size = 1;

				state = FLAG_RCV;
			} else {
				size = 0;

				state = START;
			}
			break;
		case C_RCV:
			if (c == (message[1] ^ message[2])) {
				if (DEBUG_MODE)
					printf("C_RCV: BCC received. Going to BCC_OK.\n");

				message[size++] = c;

				state = BCC_OK;
			} else if (c == FLAG) {
				if (DEBUG_MODE)
					printf("C_RCV: FLAG received. Going back to FLAG_RCV.\n");

				size = 1;

				state = FLAG_RCV;
			} else {
				if (DEBUG_MODE)
					printf("C_RCV: ? received. Going back to START.\n");

				size = 0;

				state = START;
			}
			break;
		case BCC_OK:
			if (c == FLAG) {
				if (msg->type == INVALID)
					msg->type = COMMAND;

				message[size++] = c;

				state = STOP;

				if (DEBUG_MODE)
					printf("BCC_OK: FLAG received. Going to STOP.\n");
			} else if (c != FLAG) {
				if (msg->type == INVALID)
					msg->type = DATA;
				else if (msg->type == COMMAND) {
					printf("WANING?? something unexpected happened.\n");
					state = START;
					continue;
				}

				// if writing at the end and more bytes will still be received
				if (size % ll->messageDataMaxSize == 0) {
					int mFactor = size / ll->messageDataMaxSize + 1;
					message = (unsigned char*) realloc(message,
							mFactor * ll->messageDataMaxSize);
				}

				message[size++] = c;
			}
			break;
		case STOP:
			message[size] = 0;
			done = TRUE;
			break;
		default:
			break;
		}
	}

	size = destuff(&message, size);

	unsigned char A = message[1];
	unsigned char C = message[2];
	unsigned char BCC1 = message[3];

	if (BCC1 != (A ^ C)) {
		printf("ERROR: invalid BCC1.\n");

		free(message);

		msg->type = INVALID;
		msg->error = BCC1_ERROR;

		return msg;
	}

	if (msg->type == COMMAND) {
		// get message command
		msg->command = getCommandWithControlField(message[2]);

		// get command control field
		ControlField controlField = message[2];

		char commandStr[MAX_SIZE];
		getCommandControlField(commandStr, msg->command);

		if (DEBUG_MODE)
			printf("Received command: %s.\n", commandStr);

		if (msg->command == RR || msg->command == REJ)
			msg->nr = (controlField >> 7) & BIT(0);

		if (msg->command == REJ)
			ll->stats->numReceivedREJ++;
		else if (msg->command == RR)
			ll->stats->numReceivedRR++;
	} else if (msg->type == DATA) {
		msg->data.messageSize = size - MESSAGE_SIZE;

		unsigned char calcBCC2 = processBCC(&message[4], msg->data.messageSize);
		unsigned char BCC2 = message[4 + msg->data.messageSize];

		if (calcBCC2 != BCC2) {
			printf("ERROR: invalid BCC2: 0x%02x != 0x%02x.\n", calcBCC2, BCC2);

			free(message);

			msg->type = INVALID;
			msg->error = BCC2_ERROR;

			return msg;
		}

		msg->ns = (message[2] >> 6) & BIT(0);

		// copy message
		msg->data.message = malloc(msg->data.messageSize);
		memcpy(msg->data.message, &message[4], msg->data.messageSize);

		ll->stats->receivedMessages++;
	}

	free(message);

	return msg;
}

int messageIsCommand(Message* msg, Command command) {
	return msg->type == COMMAND && msg->command == command;
}

ui stuff(unsigned char** buf, ui bufSize) {
	ui newBufSize = bufSize;

	int i;
	for (i = 1; i < bufSize - 1; i++)
		if ((*buf)[i] == FLAG || (*buf)[i] == ESCAPE)
			newBufSize++;

	*buf = (unsigned char*) realloc(*buf, newBufSize);

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

ui destuff(unsigned char** buf, ui bufSize) {
	int i;
	for (i = 1; i < bufSize - 1; ++i) {
		if ((*buf)[i] == ESCAPE) {
			memmove(*buf + i, *buf + i + 1, bufSize - i - 1);

			bufSize--;

			(*buf)[i] ^= 0x20;
		}
	}

	*buf = (unsigned char*) realloc(*buf, bufSize);

	return bufSize;
}

unsigned char processBCC(const unsigned char* buf, ui size) {
	unsigned char BCC = 0;

	ui i = 0;
	for (; i < size; i++)
		BCC ^= buf[i];

	return BCC;
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
