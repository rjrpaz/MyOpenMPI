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

#include "common.h"

//#define DEBUG 1
#undef DEBUG

#define CHUNK 1024
//#define RETRY 3
#define BUF_SIZE ETH_FRAME_TOTALLEN

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

void sigint(int);
void usage(char *);
void timeout_func(int);


int s = 0;						/*Socketdescriptor */

//void *buffer = NULL;
long total_sent_packets = 0;
unsigned long total_file_read = 0;

unsigned long timeout_count = 0;
int filefd = 0;					/* Descriptor del archivo que se va a transferir */

int main(int argc, char *argv[])
{
	/* Variable asociadas al paquete SALIENTE */

	/* buffer que contiene paquete enviado por red */
	void *buffer_sent = NULL;
	buffer_sent = (void *) malloc(BUF_SIZE);

	/* Puntero a la cabecera ethernet del paquete ENVIADO */
	unsigned char *etherhead = buffer_sent;

	/* Puntero a la seccion de datos en el paquete SR */
	unsigned char *data = buffer_sent + ETH_HEADER_LEN;

	/* Puntero a la estructura que almacena la información de la cabecera ethernet del paquete ENVIADO */
	struct ethhdr *eh = (struct ethhdr *) etherhead;

	/* 6 bytes con la dirección ethernet ORIGEN en el paquete enviado */
	unsigned char src_mac[6];

	int nonblock = 1;

	/* 6 bytes con la dirección ethernet DESTINO en el paquete enviado - estática */
	// Consulfem
	//unsigned char dest_mac[6] = {0x00, 0x19, 0xD1, 0x99, 0x79, 0xB6};
	// Casa wlan0
	// unsigned char dest_mac[6] = {0x00, 0x22, 0x43, 0x0d, 0x5c, 0x4b};
	// Documentacion
	unsigned char dest_mac[6] = {0x00, 0x0D, 0x87, 0xB8, 0x97, 0x56};
	// Boxhost
	// unsigned char dest_mac[6] = {0x00, 0x08, 0xA1, 0x74, 0x02, 0xa8};

	/* largo de paquete enviado */
	int length_sent = 0;

	/* Variables para almacenar la direcciones ethernet de cada paquete transferido */
	struct ifreq ifr;
	struct sockaddr_ll socket_address;
	/* Indice de la interfaz Ethernet local */
	int ifindex = 0;


	/* Variable asociadas al paquete ENTRANTE */

	/* buffer que contiene paquete leido */
	void *buffer_recv = NULL;
	buffer_recv = (void *) malloc(BUF_SIZE);

	/* Puntero a la cabecera ethernet del paquete RECIBIDO */
	unsigned char *etherhead_recv = buffer_recv;

	/* Puntero a la estructura que almacena la información de la cabecera ethernet del paquete RECIBIDO */
	struct ethhdr *eh_recv = (struct ethhdr *) etherhead_recv;

	/* Puntero a la seccion de datos en el paquete RECIBIDO */
	unsigned char *data_recv = buffer_recv + ETH_HEADER_LEN;

	/* largo de paquete recibido */
	int length_recv = 0;

	/* nombre del host servidor - SIN USO POR AHORA */
	char host[BUF_SIZE];


	/* Variable asociadas al archivo a copiar */

	/* buffer que contiene chunk de archivo leido */
	void *buffer_read = NULL;
	buffer_read = (void *) malloc(BUF_SIZE);

	/* largo del chunk de archivo leído */
	int length_read = 0;

	/* path y nombre del archivo */
	char path[BUF_SIZE], filename[BUF_SIZE];
	char *parse;


	/* Variables asociadas a la medición de tiempos */
	struct timeval begin;
	struct timeval end;
	//struct timeval result;
	unsigned long long allovertime;


	/* byte en la cabecera del protocolo de datos propio sobre SR */
	unsigned char nro_secuencia_enviado = 0, nro_secuencia_recibido = 0;


	unsigned int paso = 0;

	/* Variable asociada a las pruebas de retry */
	int nro_retry_congelado = 0, seguir_esperando_ack = 1, leer_siguiente_chunk = 1, cantidad_de_reintentos = 0;
	/*
	 * Almacena la cantidad de paquetes entrantes
	 * desde el último paquete socket raw detectado.
	 */
	int cantidad_paquetes_entrantes = 0;

	/*
	 * Buffer nulo, para realizar comparaciones
	 */
	void *null_buffer = NULL;
	null_buffer = malloc(BUF_SIZE);
	memset(null_buffer, 0x0, BUF_SIZE);

	int i;
	fd_set fds;
	struct timeval tv;
	int retval;

	/* COMIENZO DEL PROGRAMA */
	if (argc != 3)
		usage(argv[0]);

	sprintf(host, "%s", argv[1]);
	sprintf(path, "%s", argv[2]);

	parse = malloc(BUF_SIZE);
	sprintf(parse, "%s", path);
	parse = strtok(parse, "/");
	while (parse != NULL) {
		sprintf(filename, "%s", parse);
		parse = strtok(NULL, "/");
	}


	printf("Cliente copiará archivo %s al host %s\n", filename, host);

	/* Defino función para timeout */
	//signal(SIGALRM, timeout_func);

	/* abre socket raw */
	s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (s == -1)
		error("socket():");

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
	memset(buffer_read, 0x0, BUF_SIZE);
	memset(buffer_sent, 0x0, BUF_SIZE);


	/* Defino conjunto de file descriptors a monitorear con select() */
/*
	FD_ZERO(&fds);
	FD_SET(s, &fds);
*/
	/* Abro archivo */
	if ((filefd = (open(path, O_RDONLY))) < 0)
		error("Error al intentar leer archivo local.\n");

	/* Leo archivo de forma secuencial */
	//while ((length_read = read(filefd, buffer_read, CHUNK)) > 0) {
	while (1) {
//		printf("Leer siguiente chunk: %d\n", leer_siguiente_chunk);

		if (paso == 1) {
			/* Avanza en la lectura del archivo hacia el siguiente chunk */
			if (leer_siguiente_chunk) {
				length_read = read(filefd, buffer_read, CHUNK);
#ifdef DEBUG
				printf("Se leyeron %d bytes del archivo\n", length_read);
#endif
				if (length_read > 0) {
					total_file_read += length_read;

#ifdef DEBUG
					/* Avisa si leyó menos de lo que se pidió (ultimo chunk) */
					if (length_read != CHUNK)
						printf("Se quisieron leer %d pero se leyeron %d bytes del archivo\n", CHUNK, length_read);

					printf("\nDUMP datos leidos archivo\n");
					for (i = 0; i < length_read + RAW_HEADER_LEN + 1; i++)
						printf("%02X ", *(unsigned char *) (data + i));
					printf("\nFin de DUMP\n");
#endif
				/* error en lectura de archivo */
				} else if (length_read < 0) {
					error ("read() ");
				/* EOF */
				} else {
//					printf ("Fin de archivo\n");
					paso = 2;
				}
			/* Envío chunk anterior */
			}
		}


		/* preparo buffer donde envio dato */
		memcpy((void *) buffer_sent, (void *) dest_mac, ETH_MAC_LEN);

#ifdef DEBUG
		printf("\nDUMP\n");
		for (i = 0; i < 8; i++)
			printf("%02X ", *(unsigned char *) (buffer_sent + i));
		printf("\nFin de DUMP\n");
#endif

		memcpy((void *) (buffer_sent + ETH_MAC_LEN), (void *) src_mac,
			   ETH_MAC_LEN);

#ifdef DEBUG
		printf("\nDUMP\n");
		for (i = 0; i < ETH_MAC_LEN; i++)
			printf("%02X ", *(unsigned char *) (buffer_sent + i));
		printf("\nFin de DUMP\n");
#endif
		/* Envio nombre de archivo */
		if (paso == 0) {
			data[0] = 0xFF;
			data[1] = 'C';
			length_read = strlen(filename) + 1;
//			filename[length_read] = '\0';
			printf("Enviando nombre de archivo: %s (largo del nombre %d caracteres)\n", filename, length_read);
//			length_read++;
			memcpy((void *) (data + RAW_HEADER_LEN), (void *) filename, length_read);
		} else if (paso == 1) {
			//eh->h_proto = ETH_P_NULL;
			/* Numero de secuencia */
			data[0] = nro_secuencia_enviado;

			// Debugging
			data[1] = 'C';

			/* Contenido del chunk del archivo */
//      	printf("Marca LENGTH: %d\n", length_read);
			memcpy((void *) (data + RAW_HEADER_LEN), (void *) buffer_read, length_read);
//      	printf("BufferSENT: %s\n", (char *) buffer_sent);

		} else if (paso == 2) {
			data[0] = 0xFE;
			data[1] = 'C';
			length_read = 0;
		}


#ifdef DEBUG
		printf("\nDUMP buffer_sent\n");
		for (i = 0; i < length_read + RAW_HEADER_LEN + 1; i++)
			printf("%02X ", *(unsigned char *) (buffer_sent + i));
		printf("\nFin de DUMP\n");
#endif

		/* limpio timers para contar el tiempo */
		timerclear(&begin);
		timerclear(&end);

		/* obtengo el tiempo al momento de comenzar a enviar el paquete */
		gettimeofday(&begin, NULL);


		/* Envio paquete */
#ifdef DEBUG
		printf("Enviando porción con nro. de secuencia %d\n", nro_secuencia_enviado);
#endif
		length_sent =
			sendto(s, buffer_sent, ETH_HEADER_LEN + RAW_HEADER_LEN + length_read, 0,
				   (struct sockaddr *) &socket_address,
				   sizeof(socket_address));
//		if (length_sent < 0)
		/* Ignora error si no pudo enviar por estar lleno el buffer de salida */
		if ((length_sent < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
			error("sendto(): ");
		}

		seguir_esperando_ack = 1;

		while (seguir_esperando_ack) {
			/* Espero respuesta del servidor (handshake del nro de secuencia) */
			FD_ZERO(&fds);
			FD_SET(s, &fds);

			/* Establece timeout */
			tv.tv_sec = 0;
			tv.tv_usec = TIMEOUT;

			retval = select(s+1, &fds, NULL, NULL, &tv);
			//retval = select(s+1, &fds, NULL, NULL, NULL);

			//printf("Retval: %d\n", retval);
			if (retval == -1) {
				error("select()");
			} else if (retval) {
				cantidad_paquetes_entrantes++;
				/* Recupero dato leído */
				length_recv = recvfrom(s, buffer_recv, BUF_SIZE, 0, NULL, NULL);
				if (length_recv == -1)
					error("recvfrom()");


				//printf("Recibido dato de largo %d\n", length_recv);
				/*
				 * Compruebo si el dato recibido se corresponde a un ACK. Para ello:
				 * 1) Ethertype == 0x0
				 * 2) Direccion ethernet de destino == nuestra MAC
				 * 3) Primer y único byte de datos == nro. de secuencia
				 */
				//if (eh_recv->h_proto == ETH_P_NULL && (length_recv == (ETH_HEADER_LEN + RAW_HEADER_LEN))
				if (eh_recv->h_proto == ETH_P_NULL && (length_recv == ETH_MIN_LEN)
					&&
					(memcmp
					 ((const void *) (buffer_recv + ETH_HEADER_LEN + RAW_HEADER_LEN), (const void *) null_buffer,
					  (ETH_MIN_LEN - (ETH_HEADER_LEN + RAW_HEADER_LEN))) == 0)
					&&
					(memcmp
					 ((const void *) eh_recv->h_dest, (const void *) src_mac,
					  ETH_MAC_LEN) == 0)
					) {
					/* Almaceno el dato del paquete de respuesta, en el buffer correspondiente */
					//memcmp ((const void *) &nro_secuencia_recibido, (unsigned char *) (buffer_recv + ETH_HEADER_LEN), 1);
					nro_secuencia_recibido = (unsigned int) *(data_recv);

#ifdef DEBUG
					printf("Recibido ACK para nro. de secuencia %u en el paso %d\n", nro_secuencia_recibido, paso);
#endif
					// unsigned char *data_recv = buffer_recv + ETH_HEADER_LEN;
/*
					// unsigned char *data_recv = buffer_recv + ETH_HEADER_LEN;
					printf
						("Nro. secuencia local: %u. Nro. secuencia ACK: %d. Largo: %d\n",
						 nro_secuencia, (unsigned int) *(data_recv),
						 length_recv);
*/
					/* Chequea si el nro. de secuencia es el esperado */
					/* Recibí ACK de envío de nombre de archivo */
					if ((nro_secuencia_recibido == 0xFF) && (paso == 0)) {
						printf("El nombre del archivo fue aceptado. Enviando contenido ");
						paso = 1;
						nro_secuencia_enviado = 0;
						seguir_esperando_ack = 0;

					/* Recibi ACK de EOF */
					} else if ((nro_secuencia_recibido == 0xFE) && (paso == 2)) {
						printf("\nArchivo copiado con éxito\n");
						exit(0);

					/* Recibi ACK de transmisión del contenido del archivo */
					} else {

						if (nro_secuencia_recibido == nro_secuencia_enviado) {
#ifdef DEBUG
							printf("Recibido ACK ordenado, para nro. de secuencia %u\n", nro_secuencia_enviado);
							printf("\nDUMP handshake received\n");
							for (i = 0; i < (ETH_HEADER_LEN + RAW_HEADER_LEN); i++)
								printf("%02X ", *(unsigned char *) (buffer_recv + i));
							printf("\nFin de DUMP\n");
#endif

#if 0
							/*get time after sending..... */
							gettimeofday(&end, NULL);
							/*...and calculate difference......... */
							timersub(&end, &begin, &result);

							allovertime +=
								((result.tv_sec * 1000000) + result.tv_usec);
#endif
							nro_secuencia_enviado++;

							/* Nro de secuencia 0xF0 al 0xFF está reservador para control */
							if (nro_secuencia_enviado >= 0xf0)
								nro_secuencia_enviado = 0;

							seguir_esperando_ack = 0;
							leer_siguiente_chunk = 1;
							cantidad_de_reintentos = 0;
							printf(".");

						/* nro_secuencia_recibido != nro_secuencia_enviado */
						} else {
#ifdef DEBUG
							printf("Llegó ACK desordenado. Enviado paquete con nro. de secuencia %u, y recibido ACK para nro. de secuencia %u. Reenvío último chunk.\n", nro_secuencia_enviado, nro_secuencia_recibido);
#endif
							leer_siguiente_chunk = 0;
// El siguiente genera pesadez
//							seguir_esperando_ack = 0;
						}
					} /* if ((nro_secuencia_recibido == 0xFF) && (paso == 0)) { */
					cantidad_paquetes_entrantes = 0;

				/* el paquete recibido no es ACK */
				} else {
/*
					if (eh_recv->h_proto == ETH_P_NULL) {
						printf("Parece un ACK\n");
						printf("Largo recibido: %d Largo esperado: %d\n", length_recv, (ETH_HEADER_LEN + RAW_HEADER_LEN));
					}
*/
/*
					printf("\nDUMP dirección destino\n");
					for (i = 0; i < (ETH_MAC_LEN); i++)
						printf("%02X ", *(unsigned char *) (eh_recv->h_dest + i));
					printf("\nFin de DUMP\n");
					printf("\nDUMP src mac\n");
					for (i = 0; i < (ETH_MAC_LEN); i++)
						printf("%02X ", *(unsigned char *) (src_mac + i));
					printf("\nFin de DUMP\n");
*/
					if (cantidad_paquetes_entrantes > LIMITE_PAQUETES_ENTRANTES) {
						leer_siguiente_chunk = 0;
						seguir_esperando_ack = 0;
					} else {
						cantidad_de_reintentos++;
						if (cantidad_de_reintentos > LIMITE_REINTENTOS)
							leer_siguiente_chunk = 0;
						else
							seguir_esperando_ack = 1;
					}
				}

			/* Timeout en la espera por un paquete entrante */
			} else {
				printf("Timeout en la espera por un paquete entrante\n");
				leer_siguiente_chunk = 0;
// El siguiente genera pesadez ?
				seguir_esperando_ack = 0;
			}
			//printf("Seguir LEYENDO: %d\n", seguir_esperando_ack);
		} /* while (seguir_esperando_ack) */




/*
		total_sent_packets++;

		if ((total_file_read % CHUNK) != 0)
			printf("Tamaño leído del archivo es %ld, que no es múltiplo de %d\n", total_file_read, CHUNK);
	

		memset(buffer_read, 0x0, BUF_SIZE);
		memset(buffer_sent, 0x0, BUF_SIZE);
		memset(buffer_recv, 0x0, BUF_SIZE);
*/
	} //while ((length_read = read(filefd, buffer_read, CHUNK)) > 0) {


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


void timeout_func(int signo)
{
//        char msg[101];

//        exit(0);
//      goto resend;
    timeout_count++;
    printf("Llego SIGARLM. timeout_count %ld, filefd %d, total_file_read %ld\n", timeout_count, filefd, total_file_read);
    if (lseek(filefd, total_file_read, SEEK_SET) < 0) {
        printf("No se pudo reposicionar archivo\n");
        exit(0);
    }
    return;
}

