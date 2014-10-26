#include "Aplication.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FILE_SIZE 0
#define FILE_NAME 1


int SendCtrlPackage(int fd, int C,char* fileSize,char* fileName){

	if(C==2)
		printf("\nControl Package START\n");
	if(C==3)
		printf("\n\nControl Package END\n");

	int totalBufferSize = 1 + strlen(fileSize) + strlen(fileName);

	char controlPackage[totalBufferSize];
	controlPackage[0] = C; //start = 1 end = 2

	//tamanho do ficheiro
	controlPackage[1] = 0;
	controlPackage[2] = strlen(fileSize);

	int position_act = 3;
	
	unsigned int i=0;
	for(i=0;i<strlen(fileSize);i++)
	{
		controlPackage[position_act] = fileSize[i];
		position_act++;
	}

	/*-----------------------------------------------------------*/

	// nome do ficheiro
	controlPackage[position_act] = 1;
	position_act++;
	controlPackage[position_act] = strlen(fileName);
	position_act++;

	i=0;
	for(i=0;i<strlen(fileName);i++)
	{
		controlPackage[position_act] = fileName[i];
		position_act++;
	}

	printf("Nome: %s  Tamanho do ficheiro em bytes: %s\n" ,fileName , fileSize);

	if(!llwrite(fd, &controlPackage[0], totalBufferSize))
	{
		perror("ll_write");
        free(controlPackage);
        return -1;
	}

	return 0;
}

int SendDataPackage(int fd, int seq_number, const char* buffer, int length){

	int C = 1;
	int l2 = length/256;
	int l1 = length%256;

	int packetSize = 1 + 1 + 1 + 1 + (l2 + l1); 

	char ctrlPackage[packetSize];
	char* ctrlPackage = malloc(packetSize);

	ctrlPackage[0] = C;
	ctrlPackage[1] = seq_number;
	ctrlPackage[2] = l2;
	ctrlPackage[3] = l1;
	memcpy(&ctrlPackage[4], buffer, length);

	if(!llwrite(fd, ctrlPackage, packetSize))
	{
		perror("ll_write");
        free(ctrlPackage);
        return -1;
	}

	free(ctrlPackage);
	return 0;
}

int receiveDataPackege(int fd, int* seq_number, char** buffer, int* length){

	char* receviedPackage;
    ssize_t totalSize = ll_read(fd, &receviedPackage);
    if (totalSize < 0)
    {
        perror("ll_read");
        return 0;
    }

    if(receivedPackage[0] != 1){
    	printf("Received packet is NOT a Data Package C = %d", receivedPackage[0]);
    	return 0;
    }

    int seq = (unsigned char)receivedPackage[1];

    int bufferSize = 256 * (unsigned char)receivedPackage[2] + (unsigned char)receivedPackage[3];

    *buffer = malloc(bufferSize);
    memcpy(*buffer, &receivedPackage[4], bufferSize);

    free(receivedPackage);

    *seq_number = seq;
    *length = bufferSize;
}

int receiveCtrlPackage(int fd, int* ctrl, int* fileLength, char** fileName){

	char* receivedPackage;
	ssize_t totalSize = ll_read(fd, &receviedPackage);
    if (totalSize < 0)
    {
        perror("ll_read");
        return 0;
    }

    *ctrl = receviedPackage[0];

    unsigned int numParams = 2;
    unsigned int current_index = 1
    unsigned int numOcts;

    for(int i = 0; i < numParams){

    	paramType = receviedPackage[current_index];
    	current_index++;

    	switch(paramType){
    		case FILE_SIZE:
	    		numOcts = (unsigned char)receviedPackage[current_index];
	    		current_index++;

	    		char * length = malloc(numOcts);
				memcpy(length, &receivedPackage[current_index], numOcts);

				*filelength = atoi(length);
    		break;
    		case FILE_NAME:
    			numOcts = (unsigned char)receviedPackage[current_index];
    			current_index++;

				memcpy(*fileName, &receivedPackage[current_index], numOcts);
    		break;
    	}

    	free(length);
    }

    return 0;
}

