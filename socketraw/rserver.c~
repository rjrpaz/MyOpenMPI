#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>


#include "common.h"
#include "functions.h"

int main(int argc, char *argv[])
{
	int sockfd, newsockfd;
	unsigned int clilen;
	char buffer[BUFSIZ], buf[BUFSIZ], respuesta[BUFSIZ], pathname[BUFSIZ],
		real_pathname[BUFSIZ], tamanio_cadena[BUFSIZ];
	struct sockaddr_in serv_addr, cli_addr;
	int n, so_reuseaddr = 1;
	int pid, fd = -1, flags = 0, ret = 0, largo_tamanio_cadena = 0;
	long int inicio = 0, cantidad = 0, posicion = 0;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("Error al intentar abrir socket");
	if (setsockopt
		(sockfd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr,
		 sizeof(so_reuseaddr)) < 0) {
		error("Error al intentar reconfigurar socket");
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(PORT);
	if (bind(sockfd, (struct sockaddr *) &serv_addr,
			 sizeof(serv_addr)) < 0)
		error("Error en bind");
	listen(sockfd, 5);
	for (;;) {
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0)
			error("Error en accept");

		pid = fork();
		if (pid < 0) {
			error("Error en fork");

			/* Proceso hijo */
		} else if (pid == 0) {
			close(sockfd);
			memset(buffer, 0x0, BUFSIZ);
			/* Recibe peticiones del cliente */
			while ((n = read(newsockfd, buffer, BUFSIZ - 1)) != EOF) {
				if (n < 0)
					error("Error al intentar leer en el socket");

#ifdef DEBUG
				printf("%d: Mensaje recibido en el servidor: %s\n",
					   getpid(), buffer);
#endif

				/* Solicitud de apertura de archivo */
				if (strncmp(buffer, "OPEN ", 5) == 0) {
					if (sscanf(buffer, "OPEN %s %d", pathname, &flags) !=
						EOF) {
						sprintf(real_pathname, "./%s", pathname);
						if ((fd =
							 open(real_pathname, flags,
								  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) <
							0) {
							sprintf(respuesta,
									"ERR %d No puedo abrir archivo",
									errno);
							n = write(newsockfd, respuesta,
									  strlen(respuesta));
						} else {
#ifdef DEBUG
							printf("%d: Intenta abrir archivo: %s\n",
								   getpid(), real_pathname);
#endif
							n = write(newsockfd, "OK", 2);
						}
					} else {
						sprintf(respuesta, "%s",
								"ERR Comando no reconocido");
						n = write(newsockfd, respuesta, strlen(respuesta));
					}


					/* Solicitud de cierre de archivo */
				} else if (strcmp(buffer, "CLOSE") == 0) {
					if ((ret = close(fd)) < 0) {
						sprintf(respuesta,
								"ERR %d No puedo cerrar archivo", errno);
						n = write(newsockfd, respuesta, strlen(respuesta));
					} else {
#ifdef DEBUG
						printf("%d: Intenta cerrar el archivo\n",
							   getpid());
#endif
						n = write(newsockfd, "OK", 2);
					}
					close(newsockfd);
					exit(0);

					/* Lectura de archivo */
				} else if (strncmp(buffer, "READ ", 5) == 0) {
					if (sscanf(buffer, "READ %ld %ld", &inicio, &cantidad)
						!= EOF) {
						posicion = lseek(fd, inicio, SEEK_SET);
						if (posicion < 0) {
							sprintf(respuesta,
									"ERR %d No puedo posicionarme en archivo",
									errno);
							n = write(newsockfd, respuesta,
									  strlen(respuesta));
						}

						ret = read(fd, buffer, cantidad);
#ifdef DEBUG
						printf("%d: Retorno de la operacion read: %d\n",
							   getpid(), ret);
#endif

						if (ret < 0) {
							sprintf(respuesta,
									"ERR %d No puedo leer archivo", errno);
							n = write(newsockfd, respuesta,
									  strlen(respuesta));
						} else if (ret == 0) {
#ifdef DEBUG
							printf("%d: Fin del archivo\n", getpid());
#endif
							sprintf(respuesta, "END");
							n = write(newsockfd, respuesta,
									  largo_tamanio_cadena);
						} else {
#ifdef DEBUG
							printf
								("%d: Leyo %d bytes del archivo, desde la posicion %ld\n",
								 getpid(), ret, posicion);
#endif
							sprintf(tamanio_cadena, "%d", ret);
							largo_tamanio_cadena = strlen(tamanio_cadena);
							sprintf(respuesta, "OK %d ", ret);
							memcpy((char *) respuesta +
								   largo_tamanio_cadena + 4, buffer, ret);

							n = write(newsockfd, respuesta,
									  ret + largo_tamanio_cadena + 4);
						}
					} else {
						sprintf(respuesta, "%s",
								"ERR Comando no reconocido");
						n = write(newsockfd, respuesta, strlen(respuesta));
					}

				} else if (strncmp(buffer, "WRITE ", 6) == 0) {
					if (sscanf(buffer, "WRITE %ld", &cantidad)
						!= EOF) {

						sprintf(tamanio_cadena, "%ld", cantidad);
						largo_tamanio_cadena = strlen(tamanio_cadena);
						memcpy(buf, buffer + largo_tamanio_cadena + 7,
							   cantidad);

						ret = write(fd, buf, cantidad);

#ifdef DEBUG
						printf("%d: Retorno de la operacion read: %d\n",
							   getpid(), ret);
#endif

						if (ret < 0) {
							sprintf(respuesta,
									"ERR %d No puedo escribir archivo",
									errno);
							n = write(newsockfd, respuesta,
									  strlen(respuesta));
						} else {
#ifdef DEBUG
							printf
								("%d: Leyo %d bytes del archivo, desde la posicion %ld\n",
								 getpid(), ret, posicion);
#endif
							sprintf(respuesta, "OK %d", ret);

							n = write(newsockfd, respuesta,
									  strlen(respuesta));
						}

					} else {
						sprintf(respuesta, "%s",
								"ERR Comando no reconocido");
						n = write(newsockfd, respuesta, strlen(respuesta));
					}

					/* No identifica correctamente la cadena */
				} else {
					sprintf(respuesta, "%s", "ERR Comando no reconocido");
					n = write(newsockfd, respuesta, strlen(respuesta));
				}

				if (n < 0)
					error("Error al intentar escribir en el socket");

				memset(buffer, 0x0, BUFSIZ);
			}
			exit(0);

			/* Proceso padre */
		} else {
			close(newsockfd);
		}

	}
	return 0;
}
