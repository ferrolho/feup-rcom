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
	url_content user;
	url_content password;
	url_content host;
	url_content ip;
	url_content path;
	url_content filename;
} url;

void initURL(url* url);
void deleteURL(url* url);
int parseURL(url* url, const char* str); // Parse a string with the url to create the URL structure
int getIpByHost(url* url); // gets an IP by host name

char* processElementUntilChar(char* str, char chr);
