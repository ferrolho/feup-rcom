/*Non-Canonical Input Processing*/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BAUDRATE B38400
// POSIX compliant source
#define _POSIX_SOURCE 1
#define FALSE 0
#define TRUE 1

#define FLAG 0x7E
#define A 0x03
#define C 0x03

#define MODEMDEVICE "/dev/ttyS1"

volatile int STOP=FALSE;

int main(int argc, char** argv)
{
  int fd, res;
  struct termios oldtio,newtio;
  unsigned char SET[5];
  unsigned char UA[5];

  SET[0] = FLAG;
  SET[1] = A;
  SET[2] = C;
  SET[3] = SET[1] ^ SET[2];
  SET[4] = FLAG;

  if ( (argc < 2) || 
    ((strcmp("/dev/ttyS4", argv[1])!=0) && 
     (strcmp("/dev/ttyS1", argv[1])!=0) )) {
    printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
  exit(1);
}

  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */

    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd <0) {perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
    perror("tcgetattr");
    exit(-1);
  }

  bzero(&newtio, sizeof(newtio));
  newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 5;   /* blocking read until 5 chars received */

  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) próximo(s) caracter(es)
  */

    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");

    /*testing*/
    
    res = write(fd,SET,sizeof(SET));
    printf("%d bytes written\n", res);

  /* 
    O ciclo FOR e as instruções seguintes devem ser alterados de modo a respeitar 
    o indicado no guião 
  */

    sleep(3);

   while (STOP == FALSE) {                  //loop for input 
      res = read(fd, UA, sizeof(SET));     //returns after 5 bytes have been received 

      printf("----------\n");
      printf("FLAG: %x\n", UA[0]);
      printf("A: %x\n", UA[1]);
      printf("C: %x\n", UA[2]);
      printf("BCC: %x - %x\n", UA[3], UA[1] ^ UA[2]);
      printf("FLAG: %x\n", UA[4]);
      printf("----------\n");

      // verification
      if (UA[0] != FLAG)
        printf("Flag error\n");
      else
        STOP = TRUE;

      if (UA[1] != A)
        printf("A error\n");
      else
        STOP = TRUE;

      if (UA[2] != C)
        printf("C error\n");
      else
        STOP = TRUE;

      if (UA[3] != (UA[1] ^ UA[2]))
        printf("XOR error: %x != %x\n", UA[3], UA[1] ^ UA[2]);
      else
        STOP = TRUE;

      if (UA[4] != FLAG)
        printf("Flag error\n");
      else
        STOP = TRUE;

      if (sizeof(UA) == sizeof(SET))
        STOP = TRUE;
    }

    printf("UA received successfully.\n");

   /*
    Reconfiguração da porta de série
   */

    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    close(fd);
    return 0;
  }
