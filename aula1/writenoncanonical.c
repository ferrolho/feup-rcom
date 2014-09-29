/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

int main(int argc, char** argv) {
  int fd,c, res;
  struct termios oldtio,newtio;
  char buf[255];
  int i, sum = 0, speed = 0;
  
  if (argc < 2 || (strcmp("/dev/ttyS4", argv[1]) != 0 && (strcmp("/dev/ttyS1", argv[1]) != 0))) {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
    exit(1);
  }

  /* Open serial port device for reading and writing and not as controlling tty
  because we don't want to get killed if linenoise sends CTRL-C. */
  fd = open(argv[1], O_RDWR | O_NOCTTY);

  if (fd < 0) {
    perror(argv[1]);
    exit(-1);
  }

  /* save current port settings */
  if (tcgetattr(fd, &oldtio) == -1) {
    perror("tcgetattr");
    exit(-1);
  }

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME] = 0;   /* inter-character timer unused */
  newtio.c_cc[VMIN] = 1;   /* blocking read until 5 chars received */

  /* VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
  leitura do(s) próximo(s) caracter(es) */
  tcflush(fd, TCIOFLUSH);

  if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  printf("New termios structure set\n");

  gets(buf);
  buf[strlen(buf)] = '\0';

  res = write(fd,buf,strlen(buf)+1);
  printf("buff: %s\n", buf);
  printf("%d bytes written\n", res);

  /* O ciclo FOR e as instruções seguintes devem ser alterados de modo a respeitar o indicado no guião */

  sleep(3);

  // loop for input
  while (STOP == FALSE) {
    // returns after 5 chars have been input
    res = read(fd, buf, 1);
    // so we can printf...
    buf[res] = 0;

    printf(":%s:%d\n", buf, res);
    if (buf[0] == '\0')
      STOP = TRUE;
  }

   // Reconfiguração da porta de série
  if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
    perror("tcsetattr");
    exit(-1);
  }

  close(fd);
  return 0;
}
