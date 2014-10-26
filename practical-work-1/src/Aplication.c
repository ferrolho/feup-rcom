#include "Aplication.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#define MAX_PACKET_SIZE 255

#define FILE_SIZE 0
#define FILE_NAME 1

#define START 2
#define END 3


int sendCtrlPackage(int fd, int C,char* fileSize,char* fileName){

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

int sendDataPackage(int fd, int seq_number, const char* buffer, int length){

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

int getFileSize(FILE* file)
{ 
    int file_size;
    if (fseek(file, 0L, SEEK_END) == -1)
    {
        perror("fseek");
        return -1;
    }

    file_size = ftell(file);

    return file_size;
}

char * toArray(int number){
    int n = log10(number) + 1;
    int i;
  char *numberArray = calloc(n, sizeof(char));
    for ( i = 0; i < n; ++i, number /= 10 )
    {
        numberArray[i] = number % 10;
    }
    return numberArray;
}


int sendFile(const char* port, const char* file_name)
{ 
    FILE* file = fopen(file_name, "r");
    if (!file)
    {
        perror("fopen");
        return -1;
    }

    int fd = llopen(port, TRANSMITTER);
    if (fd == -1)
    {
        perror("llopen");
        return -1;
    }

    int file_size = getFileSize(file);
    if (file_size == -1)
    {
        perror("getFileSize");
        return -1;
    }

    if (sendCtrlPackage(fd, START, toArray(file_size), file_name) != 0)
    {
        perror("sendCtrlPackage");
        return -1;
    }

    int i = 0;

    char* buffer = malloc(MAX_PACKET_SIZE);
    size_t readBytes;
    size_t writeBytes = 0;
    while ((readBytes = fread(buffer, sizeof(char), MAX_PACKET_SIZE, file)) != 0)
    {
        if (sendDataPackage(fd, (i++) % 255, buffer, readBytes) == -1)
        {
            perror("sendDataPackage");
            free(buffer);
            return -1;
        }

        writeBytes += readBytes;

        buffer = memset(buffer, 0, MAX_PACKET_SIZE);

        printf("\rProgress: %3d%%", (int)(writeBytes / (float)file_size * 100));
        fflush(stdout);
    }
    printf("\n");

    free(buffer);

    if (fclose(file) != 0)
    {
        perror("fclose");
        return -1;
    }

    if (sendCtrlPackage(fd, END, '0', "") != 0)
    {
        perror("sendCtrlPackage");
        return -1;
    }

    if (!llclose(fd))
    {
        perror("llclose");
        return -1;
    }

    return 0;
}

int receiveFile(const char* port, const char* file_name)
{
    int fd = llopen(term, RECEIVER);
    if (fd == -1)
    {
        perror("llopen");
        return -1;
    }

    int ctrl_start, fileSize;
    char * fileName;
    if (receiveCtrlPackage(fd, &ctrl_Start, &fileSize, &fileName) != 0)
    {
        perror("receiveCtrlPackage (START)");
        return -1;
    }

    if (ctrl_start != START)
    {
        printf("Control field received (%d) is not START", ctrl_start);
        return -1;
    }

    FILE* output_file = fopen(fileName, "wb");
    if(output_file == NULL)
    {
        perror("ERROR: Opening output file");
        return -1;
    }


    int total_size_read = 0;
    int seq_number = -1;
    while (total_size_read != fileSize)
    {
        char* buffer;
        int length;
        int seq_number_before = seq_number;
        if (receiveDataPackage(fd, &seq_number, &buffer, &length) != 0)
        {
            perror("receiveDataPackage");
            free(buffer);
            return -1;
        }

        if (seq_number != 0 && seq_number_before + 1 != seq_number)
        {
            ERRORF("Expected sequence number %d but got %d", seq_number_before + 1, seq_number);
            free(buffer);
            return -1;
        }

        total_size_read += length;
        fwrite(buffer, sizeof(char), length, output_file);
        free(buffer);
    }

    if (fclose(output_file) != 0)
    {
        perror("ERROR: Closing output file");
        return -1;
    }

    int ctrl_end;
    if (receiveCtrlPackage(fd, &ctrl_end, '0', "") != 0)
    {
        perror("app_receive_control_packet (end)");
        return -1;
    }

    if (ctrl_end != END)
    {
        printf("Control field received (%d) is not END", ctrl_end);
        return -1;
    }

    if (!ll_close(fd))
    {
        perror("ll_close");
        return -1;
    }

    return 0;
}