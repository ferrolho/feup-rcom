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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define FALSE 0
#define TRUE !FALSE
#define BAUDRATE B38400
#define FLAG 0x7E
#define A 0x03
#define C 0x03

enum State {
	START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP
};

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
	newtio->c_cc[VTIME] = 0;

	// blocking read until 5 chars received
	newtio->c_cc[VMIN] = 5;

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

volatile int done = FALSE;

int state = 0;
int flag = 1, conta = 1;

void handler() {
	printf("tentativa %d: timed out!\n", conta);
	flag = 1;
	done = TRUE;
	conta++;
	state = 0;
}

int main(int argc, char** argv) {
	if (argc < 2 || strcmp("/dev/ttyS4", argv[1]) != 0) {
		printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS4\n");
		exit(1);
	}

	int fd = openSerialPort(argv[1]);

	struct termios oldtio, newtio;
	saveCurrentPortSettingsAndSetNewTermios(fd, &oldtio, &newtio);

	unsigned int bufSize = 5;
	unsigned char buf[bufSize];
	buf[0] = FLAG;
	buf[1] = A;
	buf[2] = C;
	buf[3] = buf[1] ^ buf[2];
	buf[4] = FLAG;

	signal(SIGALRM, handler);

	unsigned char UA[5];
	unsigned int uaByteBeingRead = 0;

	while (conta < 4 && !done) {
		if(flag) {
			int res;
			flag = 0;
			res = write(fd, buf, sizeof(buf));
			printf("%d bytes written\n", res);

			alarm(3);
			unsigned char byte;
			res = read(fd, &byte, 1);
			printf("res: %d\n", res);

			switch(state) {

				case 0:
				if (byte == FLAG) {
					printf("byte received : %c\n", byte);
					UA[uaByteBeingRead++] = byte;
					state = 1;
					flag = 1;
				}			
				else { 
					uaByteBeingRead = 0;
					UA[uaByteBeingRead] = 0;
					state = 0;
					printf("Flag error\n");
					done = TRUE;
				}
				break;

				case 1:
				if (byte == A) {
					printf("byte received : %c\n", byte);
					UA[uaByteBeingRead++] = byte;
					state = 2;
					flag = 1;
				}			
				else { 
					uaByteBeingRead = 0;
					UA[uaByteBeingRead] = 0;
					state = 0;
					printf("A error\n");
					done = TRUE;
				}
				break;

				case 2:
				if (byte == C) {
					printf("byte received : %c\n", byte);
					UA[uaByteBeingRead++] = byte;
					state = 3;
					flag = 1;
				}			
				else { 
					uaByteBeingRead = 0;
					UA[uaByteBeingRead] = 0;
					state = 0;
					printf("C error\n");
					done = TRUE;
				}
				break;

				case 3:
				if (byte == (A ^ C)) {
					printf("byte received : %c\n", byte);
					UA[uaByteBeingRead++] = byte;
					state = 4;
					flag = 1;
				}			

				else { 
					uaByteBeingRead = 0;
					UA[uaByteBeingRead] = 0;
					printf("XOR error: %x != %x\n", UA[3], UA[1] ^ UA[2]);
					done = TRUE;
				}
				break;

				case 4:
				if (byte == FLAG) {
					printf("byte received : %c\n", byte);
					UA[uaByteBeingRead++] = byte;
					flag = 1;
					done = TRUE;
				}			

				else { 
					uaByteBeingRead = 0;
					UA[uaByteBeingRead] = 0;
					state = 0;
					printf("Flag error\n");
					done = TRUE;
				}
				break;
			}
		}
	}

	
	if (conta >= 4) {
		printf("Connection aborted!\n");
		exit(-1);
	}


	if (UA[0] != FLAG || UA[1] != A || UA[2] != C || UA[3] != (UA[1] ^ UA[2]) || UA[4] != FLAG) {
		printf("UA error\n");
		exit(-1);
	}
	else
		printf("UA received successfully!!!! \n");
	

	// Reconfiguração da porta de série
	if (tcsetattr(fd,TCSANOW,&oldtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	close(fd);
	return 0;
}
