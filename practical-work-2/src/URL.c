#include "URL.h"

void initURL(url* url) {
	memset(url->user, 0, sizeof(url_content));
	memset(url->password, 0, sizeof(url_content));
	memset(url->host, 0, sizeof(url_content));
	memset(url->path, 0, sizeof(url_content));
	memset(url->filename, 0, sizeof(url_content));
}

void deleteURL(url* url) {
	free(url);
}

const char* regExpression =
		"ftp://([([A-Za-z0-9])*:([A-Za-z0-9])*@])*([A-Za-z0-9.~-])+/([[A-Za-z0-9/~._-])+";

const char* regExprAnony = "ftp://([A-Za-z0-9.~-])+/([[A-Za-z0-9/~._-])+";

int parseURL(url* url, const char* urlStr) {
	char* tempURL, *element, *activeExpression;
	regex_t* regex;
	size_t nmatch = strlen(urlStr);
	regmatch_t pmatch[nmatch];
	int userPassMode;

	element = (char*) malloc(strlen(urlStr));
	tempURL = (char*) malloc(strlen(urlStr));

	memcpy(tempURL, urlStr, strlen(urlStr));

	if (tempURL[6] == '[') {
		userPassMode = 1;
		activeExpression = (char*) regExpression;
	} else {
		userPassMode = 0;
		activeExpression = (char*) regExprAnony;
	}

	regex = (regex_t*) malloc(sizeof(regex_t));

	int reti;
	if ((reti = regcomp(regex, activeExpression, REG_EXTENDED)) != 0) {
		perror("URL format is wrong.");
		return 1;
	}

	if ((reti = regexec(regex, tempURL, nmatch, pmatch, REG_EXTENDED)) != 0) {
		perror("URL could not execute.");
		return 1;
	}

	free(regex);

	// removing ftp:// from string
	strcpy(tempURL, tempURL + 6);

	if (userPassMode) {
		//removing [ from string
		strcpy(tempURL, tempURL + 1);

		// saving username
		strcpy(element, processElementUntilChar(tempURL, ':'));
		memcpy(url->user, element, strlen(element));

		//saving password
		strcpy(element, processElementUntilChar(tempURL, '@'));
		memcpy(url->password, element, strlen(element));
		strcpy(tempURL, tempURL + 1);
	}

	//saving host
	strcpy(element, processElementUntilChar(tempURL, '/'));
	memcpy(url->host, element, strlen(element));

	//saving url path
	char* path = (char*) malloc(strlen(tempURL));
	int startPath = 1;
	while (strchr(tempURL, '/')) {
		element = processElementUntilChar(tempURL, '/');

		if (startPath) {
			startPath = 0;
			strcpy(path, element);
		} else {
			strcat(path, element);
		}

		strcat(path, "/");
	}
	strcpy(url->path, path);

	// saving filename
	strcpy(url->filename, tempURL);

	free(tempURL);
	free(element);

	//printf("\n%s\n%s\n%s\n%s\n%s\n", url->user, url->password, url->host,	url->path, url->filename);

	return 0;
}

int getIpByHost(url* url) {
	struct hostent* h;

	if ((h = gethostbyname(url->host)) == NULL) {
		herror("gethostbyname");
		return 1;
	}

//	printf("Host name  : %s\n", h->h_name);
//	printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *) h->h_addr)));

	char* ip = inet_ntoa(*((struct in_addr *) h->h_addr));
	strcpy(url->ip, ip);

	return 0;
}

char* processElementUntilChar(char* str, char chr) {
	// using temporary string to process substrings
	char* tempStr = (char*) malloc(strlen(str));

	// calculating length to copy element
	int index = strlen(str) - strlen(strcpy(tempStr, strchr(str, chr)));

	tempStr[index] = '\0'; // termination char in the end of string
	strncpy(tempStr, str, index);
	strcpy(str, str + strlen(tempStr) + 1);

	return tempStr;
}
