#include "FTP.h"

static int connectSocket(const char* ip, int port) {
	int sockfd;
	struct sockaddr_in server_addr;

	// server address handling
	bzero((char*) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip); /*32 bit Internet address network byte ordered*/
	server_addr.sin_port = htons(port); /*server TCP port must be network byte ordered */

	// open an TCP socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket()");
		return -1;
	}

	// connect to the server
	if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr))
			< 0) {
		perror("connect()");
		return -1;
	}

	return sockfd;
}

int ftpConnect(ftp* ftp, const char* ip, int port) {
	int socketfd, bytes;
	char rd[1024];

	if ((socketfd = connectSocket(ip, port)) < 0) {
		printf("ERROR: Cannot connect socket.\n");
		return 1;
	}

	ftp->control_socket_fd = socketfd;
	ftp->data_socket_fd = 0;

	if ((bytes = read(ftp->control_socket_fd, rd, sizeof(rd))) <= 0) {
		printf("WARNING: Nothing was read.\n");
		return 1;
	}

	printf("Bytes read: %d\nInfo: %s\n", bytes, rd);
	return 0;
}

int ftpLogin(ftp* ftp, const char* user, const char* password) {
	char buf[1024];
	int bytes;

	sprintf(buf, "USER %s\r\n", user);
	if ((bytes = write(ftp->control_socket_fd, buf, strlen(buf))) <= 0) {
		printf("ERROR: No user to be send.\n");
		return 1;
	}

	printf("Bytes send: %d\nInfo: %s\n", bytes, buf);

	if ((bytes = read(ftp->control_socket_fd, buf, sizeof(buf))) <= 0) {
		printf("ERROR: No user has been received.\n");
		return 1;
	}

	sprintf(buf, "PASS %s\r\n", password);
	if ((bytes = write(ftp->control_socket_fd, buf, strlen(buf))) <= 0) {
		printf("ERROR: No user to be send.\n");
		return 1;
	}

	printf("Bytes send: %d\nInfo: %s\n", bytes, buf);

	if ((bytes = read(ftp->control_socket_fd, buf, sizeof(buf))) <= 0) {
		printf("ERROR: No user has been received.\n");
		return 1;
	}

	return 0;
}

/*int ftp_login(FTP* ftp, const char* user, const char* pass) {
 char buffer[1024] = "";
 sprintf(buffer, "USER %s\r\n", user);
 int error = ftp_send_cmd(ftp, buffer);
 if (error)
 {
 perror("ftp_send_cmd (USER)");
 return error;
 }

 sprintf(buffer, "PASS %s\r\n", pass);
 error = ftp_send_cmd(ftp, buffer);
 if (error)
 {
 perror("ftp_send_cmd (PASS)");
 return error;
 }

 return 0;
 }

 int ftp_send_cmd(FTP *ftp, const char *cmd) {
 int error = ftp_send_cmd_no_read(ftp, cmd);
 if (error)
 {
 perror("ftp_send_cmd_no_read");
 return error;
 }

 usleep(100000);

 char str[1024] = "";
 error = ftp_read(ftp, str, sizeof(str));
 if (error)
 {
 perror("ftp_read");
 return error;
 }

 if (str[0] == '4' || str[0] == '5') { // 4xx or 5xx
 return str[0];
 }

 return 0;
 }

 int ftp_send_cmd_no_read(FTP* ftp, const char* cmd) {
 ssize_t bytes_written = write(ftp->control_socket_fd, cmd, strlen(cmd));
 if (!bytes_written)
 {
 perror("write");
 return -1;
 }

 printf("%.*s\n", (int)bytes_written, cmd);

 return 0;
 }

 int ftp_read(FTP* ftp, char* cmd, size_t size) {
 ssize_t bytes_read = read(ftp->control_socket_fd, cmd, size);
 if (!bytes_read)
 {
 perror("read");
 return -1;
 }

 printf("%.*s\n", (int)bytes_read, cmd);
 return 0;
 }*/
