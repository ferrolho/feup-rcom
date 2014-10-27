#include "Utilities.h"

int getFileSize(FILE* file) {
	if (fseek(file, 0L, SEEK_END) == -1) {
		printf("ERROR: Could not get file size.\n");
		return -1;
	}

	return ftell(file);
}
