#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

void error(char *msg)
{
	if (errno == 0) {
		fprintf(stderr, msg);
	} else {
		fprintf(stderr, "Errno %d. ", errno);
		perror(msg);
	}
	exit(0);
}

void warning(char *msg)
{
	fprintf(stderr, msg);
	return;
}


void dump_buffer(unsigned char *buffer, int size)
{
	int i = 0;
//#ifdef DEBUG
	printf("\nDUMP\n");
	for (i = 0; i < size; i++)
		printf("%02X ", *(unsigned char *) (buffer + i));
	printf("\nFin de DUMP\n");
//#endif
}

