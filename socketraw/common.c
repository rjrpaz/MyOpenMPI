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

