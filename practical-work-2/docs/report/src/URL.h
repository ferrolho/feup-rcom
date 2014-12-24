#pragma once

#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include <netinet/in.h>

typedef char url_content[256];

typedef struct URL {
	url_content user; // string to user
	url_content password; // string to password
	url_content host; // string to host
	url_content ip; // string to IP
	url_content path; // string to path
	url_content filename; // string to filename
	int port; // integer to port
} url;

void initURL(url* url);
int parseURL(url* url, const char* str); // Parse a string with the url to create the URL structure
int getIpByHost(url* url); // gets an IP by host name

char* processElementUntilChar(char* str, char chr);
