#include "Aplication.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int SendCntrlPackage(int C,char* fileSizeOct,char* fileName){

	if(C==1)
		printf("\nPacote Controlo START\n");
	if(C==2)
		printf("\n\nPacote Controlo END\n");

	char controlPackage[100];
	controlPackage[0] = C; //start = 1 end = 2

	// nome do ficheiro
	controlPackage[1] = 1;
	controlPackage[2] = strlen(fileName);

	int position_act = 3;

	unsigned int i=0;
	for(i=0;i<strlen(fileName);i++)
	{
		controlPackage[position_act] = fileName[i];
		position_act++;
	}

	//tamanho do ficheiro
	controlPackage[position_act] = 0;
	position_act++;
	controlPackage[position_act] = strlen(fileSizeOct);
	position_act++;

	printf("Nome: %s  Tamanho do ficheiro em bytes: %s e em octetos : %d\n" ,fileName , fileSizeOct, strlen(fileSizeOct));

	for(i=0; i<strlen(fileSizeOct);i++)
	{
		controlPackage[position_act] = fileSizeOct[i];
		position_act++;
	}

	//llwrite

	return 0;
}
