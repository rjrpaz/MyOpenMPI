#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <asm/types.h>

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

#include <netinet/in.h>
#include <time.h>

#include <errno.h>

#include <openssl/md5.h>

#include "common.h"

//#define DEBUG 1
#undef DEBUG

//#define RETRY 3

// Timeout en segundos
//#define TIMEOUT 3

/*
 * Posibles causas para retransmitir un paquete:
 *
 * 1) Timeout en la espera de un ACK.
 */
#define TIMEOUT 500000
/*
 * 2) Se recibió una cantidad de paquetes por la interfaz,
 * no llegan paquetes Socket Raw.
 */
#define LIMITE_REINTENTOS 100
/*
 * 3) Se recibió una cantidad de paquetes de Socket Raw, y
 * el ACK no es el esperado.
 */
#define LIMITE_PAQUETES_ENTRANTES 1000

#define LOGFILE "Stats.txt"

void sigint(int);
void usage(char *);

void startOfTx(char *, int);
void endOfTx(void);
void compareChecksum(char *path);
int esperar_ack(unsigned int, unsigned int cant_ack);
int esperar_checksum(unsigned char *);

int s = 0;						/*Socketdescriptor */

//void *buffer = NULL;
long total_sent_packets = 0;
unsigned long total_file_read = 0;

unsigned long timeout_count = 0;
int filefd = 0;					/* Descriptor del archivo que se va a transferir */

void *buffer_read = NULL;
int length_read = 0;

/* Variable asociadas al paquete SALIENTE */

/* buffer que contiene paquete enviado por red */
void *buffer_sent = NULL;

/* Puntero a la cabecera ethernet del paquete ENVIADO */
unsigned char *etherhead = NULL;

/* Puntero a la seccion de datos en el paquete SR */
unsigned char *data = NULL;

/* Puntero a la estructura que almacena la información de la cabecera ethernet del paquete ENVIADO */
struct ethhdr *eh = NULL;

/* 6 bytes con la dirección ethernet ORIGEN en el paquete enviado */
unsigned char src_mac[6];

/* almacena la direccion ethernet de cada paquete transferido */
struct sockaddr_ll socket_address;

/* Conjunto de file descriptor que monitorea con select */
fd_set fds;
/* Defino valor de timeout para select */
struct timeval tv;

/* Buffer nulo, para fines comparativos */
void *null_buffer = NULL;
void *notnull_buffer = NULL;

/* Variable asociadas al paquete ENTRANTE */

/* buffer que contiene paquete recibido por red */
void *buffer_recv = NULL;

/* Puntero a la seccion de datos en el paquete recibido */
unsigned char *data_recv = NULL;

/* Variables para archivo de log */
unsigned int nro_chunk = 0;
unsigned int cantidad_reenvios = 0;
unsigned int cantidad_ack_otro = 0;
unsigned int cantidad_superar_limite = 0;
unsigned int cantidad_timeout = 0;
unsigned int cantidad_error = 0;


int main(int argc, char *argv[])
{
	int nonblock = 1, retval = 0, filesize = 0;
	unsigned int cantidad_ack = 0;
	struct stat st;

//	FILE *logFile = NULL;

	/* 6 bytes con la dirección ethernet DESTINO en el paquete enviado - estática */
	// Consulfem
	unsigned char dest_mac[6] = {0x00, 0x19, 0xD1, 0x99, 0x79, 0xB6};
	// Casa wlan0
	// unsigned char dest_mac[6] = {0x00, 0x22, 0x43, 0x0d, 0x5c, 0x4b};
	// Documentacion
	// unsigned char dest_mac[6] = {0x00, 0x0D, 0x87, 0xB8, 0x97, 0x56};
	// Boxhost
	// unsigned char dest_mac[6] = {0x00, 0x08, 0xA1, 0x74, 0x02, 0xa8};

	/* largo de paquete enviado */
	int length_sent = 0;

	/* Variables para almacenar la direcciones ethernet de cada paquete transferido */
	struct ifreq ifr;
	/* Indice de la interfaz Ethernet local */
	int ifindex = 0;


	/* Variable asociadas al paquete ENTRANTE */

	/* buffer que contiene paquete leido */
	buffer_recv = (void *) malloc(BUFFER_SIZE);

	/* Puntero a la seccion de datos en el paquete RECIBIDO */
	data_recv = buffer_recv + ETH_HEADER_LEN;

	/* nombre del host servidor - SIN USO POR AHORA */
	char host[BUFFER_SIZE];


	/* Variable asociadas al archivo a copiar */

	/* buffer que contiene chunk de archivo leido */
	// void *buffer_read = NULL;
	buffer_read = (void *) malloc(BUFFER_SIZE * CANT_CHUNKS);

	/* largo del chunk de archivo leído */
	//int length_read = 0;

	/* path y nombre del archivo */
	char path[BUFFER_SIZE], filename[BUFFER_SIZE];
	char *parse;


	/* Variables asociadas a la medición de tiempos */
	//struct timeval result;
	unsigned long long allovertime;


	/* byte en la cabecera del protocolo de datos propio sobre SR */
	unsigned char nro_secuencia_enviado = 0;


	/* Variable asociada a las pruebas de retry */
	int nro_retry_congelado = 0;

	/*
	 * Buffer nulo, para realizar comparaciones
	 */
	null_buffer = malloc(ETH_MIN_LEN);
	notnull_buffer = malloc(CANT_CHUNKS);
	memset(null_buffer, 0x0, CANT_CHUNKS);
	memset(notnull_buffer, 0xFF, CANT_CHUNKS);

	int i, largo_tramo = 0;

	int cantidad_enviada = 0;

	/* COMIENZO DEL PROGRAMA */
	if (argc != 3)
		usage(argv[0]);

	sprintf(host, "%s", argv[1]);
	sprintf(path, "%s", argv[2]);

	parse = malloc(BUFFER_SIZE);
	sprintf(parse, "%s", path);
	parse = strtok(parse, "/");
	while (parse != NULL) {
		sprintf(filename, "%s", parse);
		parse = strtok(NULL, "/");
	}


	printf("Cliente copiará archivo %s al host %s\n", filename, host);

	/* Inicializo buffer para transmion */
	buffer_sent = (void *) malloc(BUFFER_SIZE);
	etherhead = buffer_sent;
	data = buffer_sent + ETH_HEADER_LEN;
	eh = (struct ethhdr *) etherhead;


	/* abre socket raw */
	s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (s == -1)
		error("socket():");
//	printf("SOCKET File Descriptor: %d\n", s);
	printf("SOCKET: %d\n", s);

	/* Configuro el socket como non-block */
	if (ioctl(s, FIONBIO, (char *)&nonblock) < 0)
		error("ioctl()");

	/* obtiene índice de la interfaz ethernet */
	strncpy(ifr.ifr_name, DEVICE, IFNAMSIZ);
	if (ioctl(s, SIOCGIFINDEX, &ifr) == -1)
		error("SIOCGIFINDEX");
	ifindex = ifr.ifr_ifindex;

	/* obtiene la MAC address de la interfaz */
	if (ioctl(s, SIOCGIFHWADDR, &ifr) == -1)
		error("SIOCGIFINDEX");

	/* almacena la información la MAC address origen en los bytes correspondientes */
	for (i = 0; i < 6; i++) {
		src_mac[i] = ifr.ifr_hwaddr.sa_data[i];
	}

	printf
		("MAC address origen: %02X:%02X:%02X:%02X:%02X:%02X\n",
		 src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4],
		 src_mac[5]);

//	printf("SOCKET ADDRESS: %p\n", &socket_address);

	/* Crea la estructura de dirección para el Socket Raw */
	socket_address.sll_family = PF_PACKET;
	socket_address.sll_protocol = htons(ETH_P_IP);
	socket_address.sll_ifindex = ifindex;
	socket_address.sll_hatype = ARPHRD_ETHER;
	socket_address.sll_pkttype = PACKET_OTHERHOST;
	socket_address.sll_halen = ETH_ALEN;
	socket_address.sll_addr[0] = dest_mac[0];
	socket_address.sll_addr[1] = dest_mac[1];
	socket_address.sll_addr[2] = dest_mac[2];
	socket_address.sll_addr[3] = dest_mac[3];
	socket_address.sll_addr[4] = dest_mac[4];
	socket_address.sll_addr[5] = dest_mac[5];
	socket_address.sll_addr[6] = 0x00;
	socket_address.sll_addr[7] = 0x00;

	eh->h_proto = ETH_P_NULL;

	/* Defino función para salida por interrupción del programa */
	signal(SIGINT, sigint);


	allovertime = 0;
	nro_retry_congelado = 0;
	//memset(buffer_read, 0x0, BUFFER_SIZE);
	//memset(buffer_sent, 0x0, BUFFER_SIZE);

	/* preparo buffer donde envio dato */
	memcpy((void *) buffer_sent, (void *) dest_mac, ETH_MAC_LEN);
//	dump_buffer(buffer_sent, 8);

	memcpy((void *) (buffer_sent + ETH_MAC_LEN), (void *) src_mac, ETH_MAC_LEN);
//	dump_buffer(buffer_sent, ETH_MAC_LEN);


	/* Defino conjunto de file descriptors a monitorear con select() */
//	FD_ZERO(&fds);
//	FD_SET(s, &fds);
//	tv.tv_sec = 0;
//	tv.tv_usec = TIMEOUT;

	/* Transferencia del archivo */
	/* Abro archivo */
	if ((filefd = (open(path, O_RDONLY))) < 0)
		error("Error al intentar leer archivo local.\n");

	stat(path, &st);
	filesize = st.st_size;

	/* Start of transmission */
	startOfTx(filename, filesize);

	/* Abro archivo de LOG */
/*
	logFile = malloc(sizeof(FILE));
	if ((logFile = fopen(LOGFILE, "w")) == NULL)
		error("Error al intentar abrir archivo de log");
*/

/*
	if (fprintf(logFile, "'Nro. Chunk' 'Cant. Reenvios' 'Cant. ACK otro nro. secuencia' 'Cant. veces que se supero el Limite' 'Cant. veces llego timeout' 'Cant. veces dio error'\n") < 0)
		error ("fprintf()");
*/

	largo_tramo = CHUNK*CANT_CHUNKS;

	/* Leo archivo de forma secuencial */
	nro_secuencia_enviado = 0;
	while (1) {
		nro_chunk += CANT_CHUNKS;
/*
		if (fprintf(logFile, "%u", nro_chunk) < 0)
			error ("fprintf()");
*/
		/* Avanza en la lectura del archivo hacia el siguiente chunk */
		length_read = read(filefd, buffer_read, largo_tramo);

#ifdef DEBUG
		printf("Se leyeron %d bytes del archivo\n", length_read);
#endif

		/* Se leyeron bytes del archivo */
		if (length_read > 0) {
			total_file_read += length_read;

			/* Avisa si leyó menos de lo que se pidió (ultimo chunk) */
#ifdef DEBUG
			if (length_read != largo_tramo)
				printf("Se quisieron leer %d pero se leyeron %d bytes del archivo\n", CHUNK*CANT_CHUNKS, length_read);
			dump_buffer (data, length_read + RAW_HEADER_LEN + 1);
#endif

		/* error en lectura de archivo */
		} else if (length_read < 0) {
			error ("read() ");

		/* EOF */
		} else { /* length_read == 0 */
//			printf ("Fin de archivo\n");
			/* End of transmission */
			endOfTx();

			/* Checksum */
			compareChecksum(&path[0]);
			exit(0);
		}


		/* data[0] contiene numero de secuencia */

		// Debugging
		data[1] = 'C';

		/* Contenido del chunk del archivo */
		memcpy((void *) (data + RAW_HEADER_LEN), (void *) buffer_read, length_read);

		/* Envio paquete */
#ifdef DEBUG
		printf("Enviando porción con nro. de secuencia %d\n", nro_secuencia_enviado);
#endif
		retval = 0;
		cantidad_reenvios = 0;
		cantidad_ack_otro = 0;
		cantidad_superar_limite = 0;
		cantidad_timeout = 0;
		cantidad_error = 0;

		while (1) {
			cantidad_reenvios++;
			cantidad_enviada = length_read;

			/*
			 * retval: valor numérico que indica
			 *		-1: error
			 *		 0: se recibió el ACK esperado
			 *		 1: se recibió un ACK correspondiente al
			 *		    nro. de secuencia enviado inmediatamente
			 *			anterior.
			 *		 2: se superó el límite de paquetes entrantes
			 *		    antes de recibir el ACK
			 *		 3: timeout en la lectura de un paquete entrante
			 *		 4: se recibió un ACK correspondiente a otro
			 *		    nro. de secuencia, que no se corresponde con
			 *		    el paquete anterior inmediato.
			 */
	
			/* Si el valor devuelve es 1 o 3, entonces reenvío el último chunk */

			if ((retval == 1) || (retval == 3)) {
				length_sent =
					sendto(s, buffer_sent, ETH_HEADER_LEN + RAW_HEADER_LEN + ((CHUNK < cantidad_enviada) ? CHUNK : cantidad_enviada), 0,
						   (struct sockaddr *) &socket_address,
						   sizeof(socket_address));

				/* Ignora error si no pudo enviar por estar lleno el buffer de salida */
				if ((length_sent < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
					error("sendto(): ");
				}

			} else {

printf("Retval %d, legth_read %d\n", retval, length_read);
				cantidad_ack = 0;
				for (i=0; i<CANT_CHUNKS && cantidad_enviada > 0; i++) {
//printf("Reenviando chunk %d\n", i);

					/* Numero de secuencia */
					data[0] = nro_secuencia_enviado+i;

					/* Contenido del chunk del archivo */
					memcpy((void *) (data + RAW_HEADER_LEN), (void *) (buffer_read + i*CHUNK), CHUNK);

					length_sent =
						sendto(s, buffer_sent, ETH_HEADER_LEN + RAW_HEADER_LEN + ((CHUNK < cantidad_enviada) ? CHUNK : cantidad_enviada), 0,
							   (struct sockaddr *) &socket_address,
							   sizeof(socket_address));

					/* Ignora error si no pudo enviar por estar lleno el buffer de salida */
					if ((length_sent < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
						error("sendto(): ");
					}
					cantidad_enviada -= CHUNK;
					cantidad_ack++;
/*
					if (length_read < 0)
						break;
*/
				}
			}

			if ((retval = esperar_ack(nro_secuencia_enviado, cantidad_ack)) == 0) {
				break;
			} else {
				printf("ACK de secuencia número %d devolvió valor %d\n", nro_secuencia_enviado, retval);

				if (retval == 1)
					cantidad_ack_otro++;
				else if (retval == 2)
					cantidad_superar_limite++;
				// TIMEOUT, se debe retransmitir último paquete
				else if (retval == 3)
					cantidad_timeout++;
				else
					cantidad_error++;
					
				//printf("Esperando ACK de secuencia número %d\n", nro_secuencia_enviado);
			}


		} /* while (1) */
/*
		if (fprintf(logFile, " %u %u %u %u %u\n", cantidad_reenvios, cantidad_ack_otro, cantidad_superar_limite, cantidad_timeout, cantidad_error) < 0)
			error ("fprintf()");
*/


		nro_secuencia_enviado += CANT_CHUNKS;
		/* Nro de secuencia 0xF0 al 0xFF está reservador para control */
		if ((nro_secuencia_enviado + CANT_CHUNKS) >= 0xF0)
			nro_secuencia_enviado = 0;


/*
		if (fprintf(logFile, "%s", "\n") < 0)
			error ("fprintf() ");
*/
	} /* while ((length_read = read(filefd, buffer_read, CHUNK)) > 0) { */



	/* End of transmission */
//	endOfTx();
//	fclose(logFile);


	printf("Totally sent: %ld packets. Cant timeouts %lu\n",
		   total_sent_packets, timeout_count);
	return (0);
}

void sigint(int signum)
{
	/*Clean up....... */

	struct ifreq ifr;

	if (s == -1)
		return;

	strncpy(ifr.ifr_name, DEVICE, IFNAMSIZ);
	ioctl(s, SIOCGIFFLAGS, &ifr);
	ifr.ifr_flags &= ~IFF_PROMISC;
	ioctl(s, SIOCSIFFLAGS, &ifr);
	close(s);

//  free(buffer);

	printf("Client terminating....\n");

	printf("Totally sent: %ld packets. Cant timeouts %lu\n",
		   total_sent_packets, timeout_count);
	exit(0);
}

void usage(char *argv0)
{
	printf("\nuso: %s <host> <path_local>\n\n", argv0);
	printf("\nEj: %s server /etc/hosts\n\n", argv0);
	exit(EXIT_SUCCESS);
}



/**
 * Funcion startOfTx
 *
 * Esta función envía el primer paquete de la comunicación.
 * Este es un paquete de control, e indica al servidor
 * el nombre del archivo que se va a transferir. Espera
 * además por el ACK por parte del servidor.
 *
 * Parámetros:
 * filename (IN) - Nombre del archivo que se va a
 *  	transferir.
 *
 * Devuelve:
 * Esta función no tiene valor de retorno.
 *
 */
void startOfTx(char *filename, int filesize)
{
	int filename_size = 0, length_sent = 0, retval = 0;
	char size[256];

	data[0] = 0xFF;
	data[1] = 'C';
	filename_size = strlen(filename) + 1;
	printf("Enviando datos de archivo: %s (largo del nombre %d caracteres), %d bytes\n", filename, filename_size, filesize);
	memcpy((void *) (data + RAW_HEADER_LEN), (void *) filename, filename_size);
	data[filename_size+1] = ' ';

	sprintf(size, "%d", filesize);
	memcpy((void *) (data + RAW_HEADER_LEN + filename_size), (void *) size, strlen(size));

	/* Envio paquete */
#ifdef DEBUG
	printf("Enviando inicio de transmisión\n");
#endif
//	dump_buffer(buffer_sent, ETH_HEADER_LEN + RAW_HEADER_LEN + filename_size);

	while (1) {
//		dump_buffer(buffer_sent, ETH_HEADER_LEN + RAW_HEADER_LEN + filename_size);
		length_sent =
			sendto(s, buffer_sent, ETH_HEADER_LEN + RAW_HEADER_LEN + filename_size + strlen(size), 0,
				   (struct sockaddr *) &socket_address,
				   sizeof(socket_address));
//		printf("Escribí %d bytes por el socket.\n", length_sent);
		/* Ignora error si no pudo enviar por estar lleno el buffer de salida */
		// if ((length_sent < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
		if (length_sent < 0)
			error("sendto(): ");

		if ((retval = esperar_ack(255, 1)) == 0) {
			printf("El nombre del archivo fue aceptado. Enviando contenido\n");
			return;
		}
		printf("Función devuelve error %d mientras espero ACK de startOfTransmission.\n", retval);
	}

	return;
}





/**
 * Funcion endOfTx
 *
 * Esta función envía el ultimo paquete de la comunicación.
 * Este es un paquete de control, e indica al servidor
 * que se ha finalizado de enviar el archivo, y que se
 * han recibido todos los ACK esperados por parte del servidor.
 *
 * Parámetros:
 * Esta función no tiene parametros.
 *
 * Devuelve:
 * Esta función no tiene valor de retorno.
 *
 */
void endOfTx(void)
{
	int length_sent = 0, retval = 0;

	data[0] = 0xFE;
	data[1] = 'C';

	/* Envio paquete */
#ifdef DEBUG
	printf("Enviando fin de transmisión\n");
#endif
	printf("\nEnviando fin de transmisión\n");
	length_sent =
		sendto(s, buffer_sent, ETH_HEADER_LEN + RAW_HEADER_LEN, 0,
			   (struct sockaddr *) &socket_address,
			   sizeof(socket_address));
	/* Ignora error si no pudo enviar por estar lleno el buffer de salida */
	// if ((length_sent < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
	if (length_sent < 0)
		error("sendto(): ");

//	printf("Escribí %d bytes por el socket.\n", length_sent);
	while ((retval = esperar_ack(254, 1)) != 0) {
//		printf("Esperando ACK de endOfTransmission.\n");
		printf("Función devuelve error %d.\n", retval);
	}
	printf("\nArchivo copiado con éxito\n");

	return;
}

























































/**
 * Funcion esperar_ack
 *
 * Esta función espera la confirmación del servidor,
 * respecto a que ha recibido un chunk o conjunto de
 * chunks del archivo enviado desde el cliente
 *
 * Parámetros:
 * nro_secuencia (IN) - Provee el número de secuencia
 *  	que debe ser esperado en el ACK de respuesta
 *  	por parte del servidor.
 *
 * Devuelve:
 * retval: valor numérico que indica
 *		-1: error
 *		 0: se recibió el ACK esperado
 *		 1: se recibió un ACK correspondiente al
 *		    nro. de secuencia enviado inmediatamente
 *			anterior.
 *		 2: se superó el límite de paquetes entrantes
 *		    antes de recibir el ACK
 *		 3: timeout en la lectura de un paquete entrante
 *		 4: se recibió un ACK correspondiente a otro
 *		    nro. de secuencia, que no se corresponde con
 *		    el paquete anterior inmediato.
 *
 */
int esperar_ack(unsigned int nro_secuencia, unsigned int cantidad_ack)
{
	int seguir_esperando_ack = 1, cantidad_paquetes_entrantes = 0, length_recv = 0;
	int i = 0, retval = 0;
	int length_sent = 0;
	unsigned char nro_secuencia_recibido = 0;

	/* Puntero a la cabecera ethernet del paquete RECIBIDO */
	unsigned char *etherhead_recv = buffer_recv;

	/* Puntero a la estructura que almacena la información de la cabecera ethernet del paquete RECIBIDO */
	struct ethhdr *eh_recv = (struct ethhdr *) etherhead_recv;

//	printf("Esperando ACK para nro. de secuencia: %u\n", nro_secuencia);
//	printf("SOCKET: %d\n", s);

	FD_ZERO(&fds);
	FD_SET(s, &fds);
	tv.tv_sec = 0;
	tv.tv_usec = TIMEOUT;

	while (seguir_esperando_ack) {
//		FD_ZERO(&fds);
//		FD_SET(s, &fds);
		retval = select(s+1, &fds, NULL, NULL, &tv);
		if (retval == -1) {
			error("select()");
		} else if (retval) {
			cantidad_paquetes_entrantes++;

			/* Recupero dato leído */
			length_recv = recvfrom(s, buffer_recv, BUFFER_SIZE, 0, NULL, NULL);
			if (length_recv == -1)
				error("recvfrom()");
//dump_buffer(buffer_recv + ETH_HEADER_LEN + RAW_HEADER_LEN + CANT_CHUNKS, ETH_MIN_LEN - ETH_HEADER_LEN - RAW_HEADER_LEN - CANT_CHUNKS);
//				printf ("A: %d B: %d\n", ETH_HEADER_LEN + RAW_HEADER_LEN + CANT_CHUNKS, ETH_MIN_LEN - ETH_HEADER_LEN - RAW_HEADER_LEN - CANT_CHUNKS);

			/*
			 * Compruebo si el dato recibido se corresponde a un ACK.
			 * Para ello:
			 * 3) Primer y único byte de datos == nro. de secuencia
			 */
			if (
			 	/* Ethertype == 0x0 */
				(eh_recv->h_proto == ETH_P_NULL) &&
			 	/* Largo del paquete es igual al largo mínimo posible para ethernet */
				(length_recv == ETH_MIN_LEN) &&
			 	/* El contenido del paquete se completa con NULL */
				(memcmp ((const void *) (buffer_recv + ETH_HEADER_LEN + RAW_HEADER_LEN + CANT_CHUNKS), (const void *) null_buffer, (ETH_MIN_LEN - ETH_HEADER_LEN - RAW_HEADER_LEN - CANT_CHUNKS)) == 0) &&
			 	/* La dirección ethernet destino es nuestra propia dirección */
				(memcmp ((const void *) eh_recv->h_dest, (const void *) src_mac, ETH_MAC_LEN) == 0)
				) {
//exit(0);

				/* Almaceno el dato del paquete de respuesta, en el buffer correspondiente */
				nro_secuencia_recibido = (unsigned int) *(data_recv);

#ifdef DEBUG
				printf("Recibido ACK para nro. de secuencia %u\n", nro_secuencia_recibido);
#endif

				if (nro_secuencia_recibido == nro_secuencia) {
#ifdef DEBUG
					printf("Recibido ACK ordenado, para nro. de secuencia %u\n", nro_secuencia);
					dump_buffer(buffer_recv, ETH_HEADER_LEN + RAW_HEADER_LEN + cantidad_ack);
#endif
					if (nro_secuencia < 0xF0) {
						/* El ACK llegó con algún hueco */
						if ((memcmp(buffer_recv + ETH_HEADER_LEN + RAW_HEADER_LEN, notnull_buffer, cantidad_ack)) != 0) {
//							printf("\nHAY HUECOS desde el nro secuencia %d\n", nro_secuencia);
//							dump_buffer(buffer_recv + ETH_HEADER_LEN + RAW_HEADER_LEN, CANT_CHUNKS);
							for (i=0; i<cantidad_ack; i++) {
//								dump_buffer(buffer_recv + ETH_HEADER_LEN + RAW_HEADER_LEN + i, 1);
//								printf("%u ", *(unsigned char *) (buffer_recv + ETH_HEADER_LEN + RAW_HEADER_LEN + i));
								if ((*(unsigned char *) (buffer_recv + ETH_HEADER_LEN + RAW_HEADER_LEN + i)) == 0x00) {
//									printf("HUECO en la posicion %d\n", i);
									/* Numero de secuencia */
									data[0] = nro_secuencia+i;
									/* Contenido del chunk del archivo */
									memcpy((void *) (data + RAW_HEADER_LEN), (void *) (buffer_read + i*CHUNK), CHUNK);
									length_sent =
										sendto(s, buffer_sent, ETH_HEADER_LEN + RAW_HEADER_LEN + CHUNK, 0,
											   (struct sockaddr *) &socket_address,
											   sizeof(socket_address));
									/* Ignora error si no pudo enviar por estar lleno el buffer de salida */
									if ((length_sent < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
										error("sendto(): ");
									}
								}
//								length_read -= CHUNK;
							}
//							printf("\n");
						/* if ((memcmp(buffer_recv + ETH_HEADER_LEN + RAW_HEADER_LEN, notnull_buffer, cantidad_ack)) != 0) */
						} else {
							printf("\r%lu", total_file_read);
							fflush(stdout);
							return 0;
						}

//						seguir_esperando_ack = 0;
//						leer_siguiente_chunk = 1;
//						cantidad_reintentos = 0;

//						printf(".");
//						printf("\r%lu", total_file_read);
//						fflush(stdout);

					/* if (nro_secuencia < 0xF0) */
					} else {
						return 0;
					} /* if (nro_secuencia < 0xF0) */

//					return 0;

				/* nro_secuencia_recibido != nro_secuencia_enviado */
				} else {
#ifdef DEBUG
					printf("Llegó ACK desordenado. Enviado paquete con nro. de secuencia %u, y recibido ACK para nro. de secuencia %u. Reenvío último chunk.\n", nro_secuencia, nro_secuencia_recibido);
#endif
					printf("Llegó ACK desordenado. Enviado paquete con nro. de secuencia %u, y recibido ACK para nro. de secuencia %u. Reenvío último chunk.\n", nro_secuencia, nro_secuencia_recibido);
//					leer_siguiente_chunk = 0;
					// El siguiente genera pesadez
					//seguir_esperando_ack = 0;
					if ((nro_secuencia == 0x00) && (nro_secuencia_recibido == 0xEF))
						return 1;
					else if (nro_secuencia == (nro_secuencia_recibido+1*CANT_CHUNKS))
						return 1;
					else {
						return 4;
					}
				}


			/* paquete entrante no es ACK */
			} else {
				if (cantidad_paquetes_entrantes > LIMITE_PAQUETES_ENTRANTES) {
#ifdef DEBUG
					printf("Se superó la cantidad de paquetes entrantes y no llegó ACK.\n");
#endif
//					leer_siguiente_chunk = 0;
//					seguir_esperando_ack = 0;
					return 2;
/*
				} else {
					cantidad_reintentos++;
					if (cantidad_reintentos > LIMITE_REINTENTOS) {
#ifdef DEBUG
						printf("Se superó la cantidad de reintentos y no llegó ACK.\n");
#endif
						return 1;
//						leer_siguiente_chunk = 0;
//					else
//						seguir_esperando_ack = 1;
					}
*/
				printf("NO ES ACK.\n");
				}
			} /* FIN de if paquete entrante es ACK */


		/* Timeout en la espera por un paquete entrante */
		} else {
			printf("Timeout en la espera por un paquete entrante\n");
//			leer_siguiente_chunk = 0;
// El siguiente genera pesadez ?
//			seguir_esperando_ack = 0;
			return 3;
		} /* if (retval == -1) { */

	} /* while (seguir_esperando_ack) { */
	return -1;
}


void compareChecksum(char *path)
{
    unsigned char checksum[MD5_DIGEST_LENGTH], checksumRemoto[MD5_DIGEST_LENGTH];
	int i = 0;

    printf("PATH: %s\n", path);

	int length_sent = 0, retval = 0;

	data[0] = 0xFD;
	data[1] = 'C';

	/* Envio paquete */
#ifdef DEBUG
	printf("Enviando pedido de checksum\n");
#endif
	printf("Enviando pedido de checksum\n");
	length_sent =
		sendto(s, buffer_sent, ETH_HEADER_LEN + RAW_HEADER_LEN, 0,
			   (struct sockaddr *) &socket_address,
			   sizeof(socket_address));
	/* Ignora error si no pudo enviar por estar lleno el buffer de salida */
	// if ((length_sent < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
	if (length_sent < 0)
		error("sendto(): ");

    md5sum(path, &checksum[0]);

//	printf("Escribí %d bytes por el socket.\n", length_sent);
	while ((retval = esperar_checksum(&checksumRemoto[0])) != 0) {
//		printf("Esperando ACK de endOfTransmission.\n");
		printf("Función de espera de checksum devuelve error %d.\n", retval);

		/* Reenvio por error de timeout */
		if (retval == 2) {
			length_sent =
				sendto(s, buffer_sent, ETH_HEADER_LEN + RAW_HEADER_LEN, 0,
					   (struct sockaddr *) &socket_address,
					   sizeof(socket_address));
			/* Ignora error si no pudo enviar por estar lleno el buffer de salida */
			// if ((length_sent < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
			if (length_sent < 0)
				error("sendto(): ");
		}
	}

/*
	printf("CHK_SUM: %s\n", checksum);
	printf("CHK_REM: %s\n", checksumRemoto);
*/

/*
    for(n=0; n<MD5_DIGEST_LENGTH; n++)
        printf("%02x", checksum[n]);
    printf("\n");
*/

    for(i=0; i<MD5_DIGEST_LENGTH; i++) {
		if (checksum[i] != checksumRemoto[i]) {
			printf("Archivo copiado con errores\n");
			return;
		}
	}

	printf("Archivo copiado correctamente\n");
    return;
}


















int esperar_checksum(unsigned char *checksum)
{
	int seguir_esperando_ack = 1, cantidad_paquetes_entrantes = 0, length_recv = 0;
	int i = 0, retval = 0;
	unsigned char nro_secuencia_recibido = 0;

	/* Puntero a la cabecera ethernet del paquete RECIBIDO */
	unsigned char *etherhead_recv = buffer_recv;

	/* Puntero a la estructura que almacena la información de la cabecera ethernet del paquete RECIBIDO */
	struct ethhdr *eh_recv = (struct ethhdr *) etherhead_recv;

	FD_ZERO(&fds);
	FD_SET(s, &fds);
	tv.tv_sec = 10;
	tv.tv_usec = 0;

	printf("ESPERANDO CHECKSUM\n");
	while (seguir_esperando_ack) {
		retval = select(s+1, &fds, NULL, NULL, &tv);
		if (retval == -1) {
			error("select()");
		} else if (retval) {
			cantidad_paquetes_entrantes++;

			/* Recupero dato leído */
			length_recv = recvfrom(s, buffer_recv, BUFFER_SIZE, 0, NULL, NULL);
			if (length_recv == -1)
				error("recvfrom()");

			/*
			 * Compruebo si el dato recibido se corresponde a un ACK.
			 * Para ello:
			 * 3) Primer y único byte de datos == nro. de secuencia
			 */
			if (
			 	/* Ethertype == 0x0 */
				(eh_recv->h_proto == ETH_P_NULL) &&
			 	/* Largo del paquete es igual al largo mínimo posible para ethernet */
				(length_recv == ETH_MIN_LEN) &&
			 	/* El contenido del paquete se completa con NULL */
//				(memcmp ((const void *) (buffer_recv + ETH_HEADER_LEN + RAW_HEADER_LEN + MD5_DIGEST_LENGTH), (const void *) null_buffer, (ETH_MIN_LEN - ETH_HEADER_LEN - RAW_HEADER_LEN - MD5_DIGEST_LENGTH)) == 0) &&
			 	/* La dirección ethernet destino es nuestra propia dirección */
				(memcmp ((const void *) eh_recv->h_dest, (const void *) src_mac, ETH_MAC_LEN) == 0)
				) {

dump_buffer(buffer_recv + ETH_HEADER_LEN, RAW_HEADER_LEN + MD5_DIGEST_LENGTH);
				/* Almaceno el dato del paquete de respuesta, en el buffer correspondiente */
				nro_secuencia_recibido = (unsigned int) *(data_recv);

				printf("CHECKSUM. Recibido ACK para nro. de secuencia %u\n", nro_secuencia_recibido);
#ifdef DEBUG
				printf("Recibido ACK para nro. de secuencia %u\n", nro_secuencia_recibido);
#endif

				if (nro_secuencia_recibido == 0xFD) {
#ifdef DEBUG
					printf("Recibido ACK ordenado, para nro. de secuencia %u\n", nro_secuencia);
					dump_buffer(buffer_recv, ETH_HEADER_LEN + RAW_HEADER_LEN + cantidad_ack);
#endif
					for (i=0; i<MD5_DIGEST_LENGTH; i++)
						checksum[i] = *(unsigned char *) (buffer_recv + ETH_HEADER_LEN + RAW_HEADER_LEN + i);

//					printf("\nChecksum: %s", checksum);
					fflush(stdout);

					return 0;

				/* nro_secuencia_recibido != nro_secuencia_enviado */
				} else {
#ifdef DEBUG
					printf("Llegó ACK desordenado. Enviado paquete con nro. de secuencia %u, y recibido ACK para nro. de secuencia %u. Reenvío último chunk.\n", nro_secuencia, nro_secuencia_recibido);
#endif
					return 1;
				}


			/* paquete entrante no es ACK */
			} else {
				if (cantidad_paquetes_entrantes > LIMITE_PAQUETES_ENTRANTES) {
//					printf("Se superó la cantidad de paquetes entrantes y no llegó ACK.\n");
#ifdef DEBUG
					printf("Se superó la cantidad de paquetes entrantes y no llegó ACK.\n");
#endif
//				printf("NO ES ACK.\n");
					return 2;
				}
			} /* FIN de if paquete entrante es ACK */


		/* Timeout en la espera por un paquete entrante */
		} else {
			return 3;
		} /* if (retval == -1) { */

	} /* while (seguir_esperando_ack) { */
	return -1;
}


