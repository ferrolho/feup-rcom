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

