/* Non-Canonical Input Processing */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BAUDRATE B38400
#define FALSE 0
#define TRUE 1

#define FLAG 0x7E
#define A 0x03
#define C 0x03

volatile int STOP = FALSE;

enum STATE {
  start, flag_rcv, a_rcv, c_rcv, bcc_ok, stop
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

int main(int argc, char** argv) {
  if (argc < 2 || strcmp("/dev/ttyS0", argv[1]) != 0) {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS0\n");
    exit(1);
  }

  int fd = openSerialPort(argv[1]);

  struct termios oldtio, newtio;

  // save current port settings
  if (tcgetattr(fd, &oldtio) == -1) {
    perror("tcgetattr");
    exit(-1);
  }

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;
  newtio.c_lflag = 0;
  // inter-character timer unused
  newtio.c_cc[VTIME] = 0;
  // blocking read until 5 chars received
  newtio.c_cc[VMIN] = 5;

  tcflush(fd, TCIOFLUSH);
  if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  } else
  printf("New termios structure set.\n");

  // loop for input
  unsigned char buf[5];
  while (STOP == FALSE) {
    read(fd, buf, sizeof(buf));

    printf("----------\n");
    printf("FLAG: %x\n", buf[0]);
    printf("A: %x\n", buf[1]);
    printf("C: %x\n", buf[2]);
    printf("BCC: %x - %x\n", buf[3], buf[1] ^ buf[2]);
    printf("FLAG: %x\n", buf[4]);
    printf("----------\n");

    // verification
    if (buf[0] != FLAG)
      printf("Flag error\n");
    else
      STOP = TRUE;

    if (buf[1] != A)
      printf("A error\n");
    else
      STOP = TRUE;

    if (buf[2] != C)
      printf("C error\n");
    else
      STOP = TRUE;

    if (buf[3] != (buf[1] ^ buf[2]))
      printf("XOR error: %x != %x\n", buf[3], buf[1] ^ buf[2]);
    else
      STOP = TRUE;

    if (buf[4] != FLAG)
      printf("Flag error\n");
    else
      STOP = TRUE;
  }

  printf("SET received successfully.\n");
  printf("Retrieving UA.\n");
  write(fd, buf, sizeof(buf));

  tcsetattr(fd, TCSANOW, &oldtio);
  close(fd);

  return 0;
}
