#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>

#include "common.h"

#define DEBUG 1
//#undef DEBUG

void error(char *);
void warning(char *);

extern long unsigned int timeout_count, total_file_read;
extern int filefd;

int r_open(const char *host, const char *pathname, int flags)
{
	int sockfd, n;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	char buffer[BUFSIZ], merror[BUFSIZ];

	/* Establece conexion con el servidor */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("Error al intentar abrir socket");

	server = gethostbyname(host);
	if (server == NULL)
		error("No pudo determinarse la identidad del host\n");

	serv_addr.sin_family = AF_INET;
	memcpy((char *) &serv_addr.sin_addr.s_addr, server->h_addr_list[0],
		   server->h_length);
	serv_addr.sin_port = htons(PORT);
	if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))
		< 0)
		error("Error al intentar conectarse al servidor");

	/* Envia peticion de apertura de archivo en el servidor */
	sprintf(buffer, "OPEN %s %d", pathname, flags);

	n = write(sockfd, buffer, strlen(buffer));
	if (n < 0)
		error("Error al intentar escribir en el socket");
	memset(buffer, 0x0, BUFSIZ);
	n = read(sockfd, buffer, BUFSIZ - 1);
	if (n < 0)
		error("Error al intentar leer en el socket");


	if (strcmp(buffer, "OK") == 0) {
		return (sockfd);
	} else {
		if (sscanf(buffer, "ERR %d %s", &errno, merror) != EOF) {
//          warning(merror);
		}
		return (-1);
	}

	return (sockfd);
}


int r_close(int sd)
{
	int n = 0;
	char buffer[BUFSIZ], merror[BUFSIZ];

	/* Envia peticion de apertura de archivo en el servidor */
	memset(buffer, 0x0, BUFSIZ);
	sprintf(buffer, "%s", "CLOSE");

	n = write(sd, buffer, strlen(buffer));
	if (n < 0)
		error("Error al intentar escribir en el socket");
	memset(buffer, 0x0, BUFSIZ);
	n = read(sd, buffer, BUFSIZ - 1);
	if (n < 0)
		error("Error al intentar leer en el socket");

	if (strcmp(buffer, "OK") == 0) {
		close(sd);
		return (0);
	} else {
		if (sscanf(buffer, "ERR %d %s", &errno, merror) != EOF) {
//          warning(merror);
		}
		return (-1);
	}
}

ssize_t r_read(int sd, void *buf, size_t posicion, size_t count)
{
	char tamanio_cadena[BUFSIZ], buffer[BUFSIZ], merror[BUFSIZ];
	long int largo_tamanio_cadena = 0;
	int n = 0, ret = 0, offset = 0;

	/* Envia peticion de lectura de archivo en el servidor */
	sprintf(buffer, "READ %d %d", posicion, count);

	n = write(sd, buffer, strlen(buffer));
	if (n < 0)
		error("Error al intentar escribir en el socket");
	memset(buffer, 0x0, BUFSIZ);
	n = read(sd, buffer, BUFSIZ - 1);
	if (n < 0)
		error("Error al intentar leer en el socket");

//  printf("Leido dentro de la funcion: %d\n", n);
//  printf("largo cadena dentro de la funcion: %d\n", strlen(buffer));
//  printf("Cadena completa dentro de la funcion:\n%s\n", buffer);

	if (strncmp(buffer, "OK ", 3) == 0) {
		offset = 3;
		if (sscanf(buffer, "OK %d", &ret) != EOF) {
			sprintf(tamanio_cadena, "%d", ret);
			largo_tamanio_cadena = strlen(tamanio_cadena);
			offset = offset + largo_tamanio_cadena + 1;
			memcpy(buf, buffer + offset, ret);
			return (ret);
		}

		return (-1);

	} else if (strcmp(buffer, "END") == 0) {
		return (EOF);

	} else {
		if (sscanf(buffer, "ERR %d %s", &errno, merror) != EOF) {
//          warning(merror);
		}
		return (-1);

	}
}


ssize_t r_write(int sd, void *buf, size_t count)
{
	char tamanio_cadena[BUFSIZ], buffer[BUFSIZ], merror[BUFSIZ];
	long int largo_tamanio_cadena = 0;
	int n = 0, ret = 0, offset = 0;

	/* Envia peticion de apertura de archivo en el servidor */

	sprintf(buffer, "WRITE %d ", count);

	offset = 7;
	sprintf(tamanio_cadena, "%d", count);
	largo_tamanio_cadena = strlen(tamanio_cadena);
	offset = offset + largo_tamanio_cadena;
	memcpy(buffer + offset, buf, count);

	n = write(sd, buffer, count + offset);
	if (n < 0)
		error("Error al intentar escribir en el socket");
	memset(buffer, 0x0, BUFSIZ);
	n = read(sd, buffer, BUFSIZ - 1);
	if (n < 0)
		error("Error al intentar leer en el socket");

//  printf("Leido dentro de la funcion: %d\n", n);
//  printf("largo cadena dentro de la funcion: %d\n", strlen(buffer));
//  printf("Cadena completa dentro de la funcion:\n%s\n", buffer);

	if (strncmp(buffer, "OK ", 3) == 0) {
		if (sscanf(buffer, "OK %d", &ret) != EOF) {
//          warning(merror);
		}
		return (ret);

	} else {
		if (sscanf(buffer, "ERR %d %s", &errno, merror) != EOF) {
//          warning(merror);
		}
		return (-1);

	}
}



void error(char *msg)
{
	if (errno == 0) {
		fprintf(stderr, msg);
	} else {
		perror(msg);
	}
	exit(0);
}

void warning(char *msg)
{
	fprintf(stderr, msg);
	return;
}

void timeout_func(int signo)
{
//        char msg[101];

	printf("Llego SIGARLM\n");
//        exit(0);
//      goto resend;
	timeout_count++;
	if (lseek(filefd, total_file_read, SEEK_SET) < 0) {
		printf("No se pudo reposicionar archivo\n");
		exit(0);
	}
	return;
}


int myprintf(const char *fmt, ...) {
	va_list ap;
	va_start(ap,fmt);

                   int d;
                   char c, *s;

                   while (*fmt)
                        switch(*fmt++) {
                        case 's':           /* string */
                             s = va_arg(ap, char *);
                             printf("es string %s\n", s);
                             break;
                        case 'd':           /* int */
                             d = va_arg(ap, int);
                             printf("es int %d\n", d);
                             break;
                        case 'c':           /* char */
                             /* need a cast here since va_arg only
                                takes fully promoted types */
                             c = (char) va_arg(ap, int);
                             printf("es char %c\n", c);
                             break;
                        }
                   va_end(ap);



#ifdef DEBUG
	return(printf(fmt,ap));
#else
    return 0;
#endif
}

