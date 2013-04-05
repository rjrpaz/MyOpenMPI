#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <openssl/md5.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


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



void md5sum(char *path, unsigned char *checksum)
{
	int fd;
	MD5_CTX c;
	char buf[512];
	ssize_t bytes;

	if ((fd = (open(path, O_RDONLY))) < 0)
		error("Error al intentar leer archivo local.\n");

	MD5_Init(&c);
	bytes=read(fd, buf, 512);
	while(bytes > 0) {
		MD5_Update(&c, buf, bytes);
		bytes=read(fd, buf, 512);
	}
	MD5_Final(checksum, &c);

	close(fd);

/*
	int n=0;
        for(n=0; n<MD5_DIGEST_LENGTH; n++)
                printf("%02x ", *(checksum + n));
        printf("\n");
*/

	return;
}


