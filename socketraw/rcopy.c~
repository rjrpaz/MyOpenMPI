#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEBUG 1

#include "functions.h"

int main(int argc, char *argv[])
{
	int fd = 0, filefd = 0, posicion = 0, cantidad = 1024, ret =
		0, is_dir = 0;
	char buffer[BUFSIZ], *result, rhost[BUFSIZ], rpath[BUFSIZ],
		origen[BUFSIZ], destino[BUFSIZ];
	struct stat stat_destino;

	if (argc != 3) {
		printf("\nuso: %s <origen> <destino>\n\n", argv[0]);
		printf("\nEj: %s host:path_remoto path_local\n\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	/* Origen es remoto */
	if ((strchr(argv[1], ':')) != NULL) {
		result = strtok(argv[1], ":");
		sprintf(rhost, "%s", result);
		result = strtok(NULL, ":");
		sprintf(rpath, "%s", result);

		result = strtok(argv[2], ":");
		result = strtok(NULL, ":");
		/* Destino es remoto. Aborta operacion */
		if (result != NULL) {
			error("Origen y destino no pueden ser remotos.\n");

			/* Destino es local */
		} else {
			/* Destino es un directorio */
			if ((ret = stat(argv[2], &stat_destino)) < 0) {
				if (errno != ENOENT) {
					error("Error al abrir o crear destino");
				}
			}
			is_dir = S_ISDIR(stat_destino.st_mode);
			/* Destino es directorio. Anexo el nombre de archivo */
			if (is_dir) {
				sprintf(destino, "%s/%s", argv[2], rpath);
				/* Destino es archivo */
			} else {
				sprintf(destino, "%s", argv[2]);
			}

#ifdef DEBUG
			printf("Origen remoto: %s:%s\tDestino local: %s\n", rhost,
				   rpath, destino);
#endif
			if ((filefd =
				 (open
				  (destino, O_CREAT | O_WRONLY | O_TRUNC,
				   S_IRUSR | S_IWUSR))) < 0) {
				error("Error al intentar generar archivo local.\n");
			}

			fd = r_open(rhost, rpath, O_RDONLY);
			if (fd < 0)
				error("No puedo abrir archivo remoto");

			memset(buffer, 0x0, BUFSIZ);
			while ((ret = r_read(fd, buffer, posicion, cantidad)) != EOF) {
				write(filefd, buffer, ret);
				posicion += ret;
			}
			close(filefd);

			r_close(fd);
		}


		/* Origen es local */
	} else {
		sprintf(origen, "%s", argv[1]);
		result = strtok(argv[2], ":");
		sprintf(rhost, "%s", result);
		result = strtok(NULL, ":");

		printf("Remote host: %s\n", rhost);
		/* Destino es remoto */
		if (result != NULL) {
			sprintf(rpath, "%s", result);

			if ((filefd = (open(origen, O_RDONLY))) < 0) {
				error("Error al intentar leer archivo local.\n");
			}

			sprintf(destino, "%s", rpath);
			fd = r_open(rhost, destino, O_WRONLY | O_CREAT | O_TRUNC);
			if (fd < 0) {
				/*
				 * Si el error es del tipo "Is a directory", reintenta
				 * anexando el nombre del archivo
				 */
				if (errno == EISDIR) {
					sprintf(destino, "%s/%s", rpath, origen);
					printf("Destino: %s\n", destino);
					fd = r_open(rhost, destino, O_WRONLY | O_CREAT);
					if (fd < 0)
						error("No puedo abrir archivo remoto");
				} else {
					error("No puedo abrir archivo remoto");
				}
			}
#ifdef DEBUG
			printf("Origen local: %s\tDestino remoto: %s:%s\n", origen,
				   rhost, destino);
#endif

			memset(buffer, 0x0, BUFSIZ);
			while ((ret = read(filefd, buffer, cantidad)) > 0) {
				r_write(fd, buffer, ret);
				memset(buffer, 0x0, BUFSIZ);
			}
			if (ret == -1)
				error("Problemas en lectura de archivo local");

			close(filefd);

			r_close(fd);

			/* Destino es local. Aborta operacion */
		} else {
			error("Origen y destino no pueden ser locales.\n");
		}

	}

	exit(0);
}
