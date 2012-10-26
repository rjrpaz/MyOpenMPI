#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>

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

#include "common.h"

//#define DEBUG 1
#undef DEBUG

#define TIMEOUT 500000
//#define TIMEOUT 0
#define LIMITE_REINTENTOS 1000

#define PATH_DESTINO "/tmp"

//void timeout_func(int);
void sigint(int signum);

//void startOfRx(void);

int enviar_ack(unsigned char *etherhead, unsigned char nro_secuencia);
int distancia(unsigned char, unsigned char);

int s = 0, filefd = 0;
long total_packets = 0;
long answered_packets = 0;

/* 6 bytes con la dirección ethernet ORIGEN en el paquete de ACK enviado */
unsigned char src_mac[6];

struct sockaddr_ll socket_address;

/* Variable asociadas al paquete SALIENTE */

/* buffer que contiene paquete enviado */
void *buffer_sent = NULL;

/* Puntero a la cabecera ethernet del paquete ENVIADO */
unsigned char *etherhead_sent = NULL;

/* Puntero a la estructura que almacena la información de la cabecera ethernet del paquete ENVIADO */
struct ethhdr *eh_sent = NULL;

/* Variable asociadas al paquete ENTRANTE */

/* buffer que contiene paquete recibido por red */
void *buffer_recv = NULL;

/* Puntero a la cabecera ethernet del paquete RECIBIDO */
unsigned char *etherhead = NULL;

/* Puntero a la estructura que almacena la información de la cabecera ethernet del paquete RECIBIDO */
struct ethhdr *eh = NULL;

/* Puntero a la seccion de datos en el paquete SR */
unsigned char *data;

int main(void)
{
	char serverID = 'S';

	/* Variable asociadas al paquete ENTRANTE */

	buffer_recv = (void *) malloc(BUFFER_SIZE);

	/* Puntero a la cabecera ethernet del paquete RECIBIDO */
	etherhead = buffer_recv;

	/* Puntero a la estructura que almacena la información de la cabecera ethernet del paquete RECIBIDO */
	eh = (struct ethhdr *) etherhead;

	int nonblock = 1;


	/* Variables para almacenar nuestra propia dirección ethernet */
	struct ifreq ifr;
	/* Indice de la interfaz Ethernet local */
	int ifindex = 0;


	/* Variables asociadas al paquete SALIENTE */

	buffer_sent = (void *) malloc(BUFFER_SIZE);
	etherhead_sent = buffer_sent;
	eh_sent = (struct ethhdr *) etherhead_sent;


	// REVISAR
	int length;					/*length of received packet */
	int sent;
	char path[BUFFER_SIZE];
	unsigned char nro_secuencia = 0xEF, nro_secuencia_anterior = 0xEF;

	/* Variables asociadas a la ESCRITURA del archivo */

	void *buffer_write = NULL;
	buffer_write = (void *) malloc(BUFFER_SIZE*CANT_CHUNKS);	/*Buffer for Ethernet Frame */

	unsigned int primer_paquete = 1, posicion = 0, saldo = 0;
//	unsigned char anterior = 0xEF, nuevo = 0xEF;
	unsigned char nuevo = 0xEF;

	/* Variables asociada a las pruebas de retry */
	int i;
	fd_set rfds;
	struct timeval tv;
	int retval, cantidad_de_reintentos = 0;

	/* Buffer en 0xFF, para fines comparativos */
	void *null_buffer = NULL, *notnull_buffer = NULL;
	null_buffer = malloc(CANT_CHUNKS);
	notnull_buffer = malloc(CANT_CHUNKS);
    memset(null_buffer, 0x00, CANT_CHUNKS);
    memset(notnull_buffer, 0xFF, CANT_CHUNKS);


	/* COMIENZO DEL PROGRAMA */
	printf("Arrancando servidor\n");


	/* abre socket raw */
	s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (s == -1)
		error("socket():");

	/* obtiene índice de la interfaz ethernet */
	strncpy(ifr.ifr_name, DEVICE, IFNAMSIZ);
	if (ioctl(s, SIOCGIFINDEX, &ifr) == -1)
		error("SIOCGIFINDEX");
	ifindex = ifr.ifr_ifindex;

	/* obtiene la MAC address de la interfaz */
	if (ioctl(s, SIOCGIFHWADDR, &ifr) == -1)
		error("SIOCGIFINDEX");

	/* almacena la información de nuestra propia MAC address en los bytes correspondientes */
	for (i = 0; i < 6; i++) {
		src_mac[i] = ifr.ifr_hwaddr.sa_data[i];
	}

	printf
		("MAC address local: %02X:%02X:%02X:%02X:%02X:%02X\n",
		 src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4],
		 src_mac[5]);


	/* Crea la estructura de dirección para el Socket Raw */
	socket_address.sll_family = PF_PACKET;
	socket_address.sll_protocol = htons(ETH_P_IP);
	socket_address.sll_ifindex = ifindex;
	socket_address.sll_hatype = ARPHRD_ETHER;
	socket_address.sll_pkttype = PACKET_OTHERHOST;
	socket_address.sll_halen = ETH_ALEN;
	socket_address.sll_addr[6] = 0x00;
	socket_address.sll_addr[7] = 0x00;

	memcpy((void *) (buffer_sent + ETH_HEADER_LEN + 1), (const void *) &serverID, 1);
	memcpy((void *) (etherhead_sent + ETH_MAC_LEN), (const void *) src_mac, ETH_MAC_LEN);

	/*prepare sockaddr_ll */
	socket_address.sll_addr[0] = eh_sent->h_dest[0];
	socket_address.sll_addr[1] = eh_sent->h_dest[1];
	socket_address.sll_addr[2] = eh_sent->h_dest[2];
	socket_address.sll_addr[3] = eh_sent->h_dest[3];
	socket_address.sll_addr[4] = eh_sent->h_dest[4];
	socket_address.sll_addr[5] = eh_sent->h_dest[5];

	/* Defino función para salida por interrupción del programa */
	signal(SIGINT, sigint);

	printf ("Esperando paquetes entrantes\n");


	/* startOfRx */
//	startOfRx();

	/* Configuro el socket como non-block */
	if (ioctl(s, FIONBIO, (char *)&nonblock) < 0)
		error("ioctl()");

	long int total_escrito = 0;

	while (1) {
		/* Espero paquete entrante */
		FD_ZERO(&rfds);
		FD_SET(s, &rfds);

		/* Establece timeout */
		tv.tv_sec = 0;
		tv.tv_usec = TIMEOUT;

		retval = select(s+1, &rfds, NULL, NULL, &tv);

		if (retval == -1)
			error("select()");
		else if (retval) {
			//printf("Se detectó paquete entrante antes del timeout\n");
			/* Recupero dato leído */
			length = recvfrom(s, buffer_recv, BUFFER_SIZE, 0, NULL, NULL);
			if (length == -1)
				error("recvfrom():");

			/*
			 * Compruebo si el dato recibido se corresponde a un socket raw
			 * 1) Ethertype == 0x0
			 * 2) Direccion ethernet de destino == nuestra MAC
			 * 3) Primer byte de datos == nro. de secuencia
			 */
			if (eh->h_proto == ETH_P_NULL
				&& memcmp((const void *) eh->h_dest, (const void *) src_mac,
						  ETH_MAC_LEN) == 0) {

#ifdef DEBUG
				printf("Se detectó paquete socket raw\n");
#endif

				data = buffer_recv + ETH_HEADER_LEN;
				nro_secuencia = (unsigned char) *data;

				/*
				 * Copio nro de secuencia para ACK.
				 * Si recibí el nro. de secuencia que esperaba, mando el ACK
				 * con ese número.
				 * Si recibo otra cosa, puedo suponer que se perdió un paquete
				 * asi que pido nuevamente el anterior.
				 */
				nuevo = (unsigned int) nro_secuencia;

				/* Nombre del archivo */
				if (nuevo == 0xFF) {
					memcpy((void *) buffer_write, (void *) (buffer_recv + ETH_HEADER_LEN + RAW_HEADER_LEN), length - ETH_HEADER_LEN - RAW_HEADER_LEN);
					sprintf(path, "%s/%s", PATH_DESTINO, (char *) buffer_write);
					printf("Recibiendo archivo %s\n", path);
					/* Abro archivo si no ha sido abierto */
					if (filefd == 0) {
						if ((filefd = (open(path, O_WRONLY | O_CREAT | O_TRUNC))) < 0) {
							error("Error al intentar leer archivo local.\n");
						}
					}
					//nro_secuencia_ant = 0xEF;
					nro_secuencia_anterior = 0x00;

					sent = enviar_ack(etherhead, nro_secuencia);
					if (sent == -1)
						error("enviar_ack():");

//					anterior = 0xEF;
					memset((void *) (buffer_sent + ETH_HEADER_LEN + RAW_HEADER_LEN), 0x0, CANT_CHUNKS);
				/* EOF */
				} else if (nuevo == 0xFE) {
					/*
					 * Si el buffer de ACK no está en blanco, entonces
					 * hay un saldo en el buffer que debo escribir en
					 * el archivo.
					 */
//					printf("HAY ALGO PARA ESCRIBIR?: saldo %u", saldo);
//					dump_buffer(buffer_sent + ETH_HEADER_LEN + RAW_HEADER_LEN, CANT_CHUNKS);
					if (memcmp((void *) (buffer_sent + ETH_HEADER_LEN + RAW_HEADER_LEN), (void *) null_buffer, CANT_CHUNKS) != 0) {
						write(filefd, buffer_write, saldo);
						total_escrito += saldo;
					}

					if (filefd != 0) {
						close (filefd);
						printf("Archivo %s fue recibido completamente\n", path);
					}

					memset((void *) (buffer_sent + ETH_HEADER_LEN + RAW_HEADER_LEN), 0x0, CANT_CHUNKS);
					sent = enviar_ack(etherhead, nro_secuencia);
					if (sent == -1)
						error("enviar_ack():");

					primer_paquete = 0;
					filefd = 0;

				/* Contenido del archivo */
				} else {

					/* Si envía secuencia de la siguiente ráfaga, confirma que llegó el ACK al cliente */
					int dist = distancia(nro_secuencia, nro_secuencia_anterior);
//					printf("DIST: %d, %d - %d\n", dist, nro_secuencia, nro_secuencia_anterior);
//					if (distancia(nro_secuencia, nro_secuencia_anterior) >= CANT_CHUNKS) {
					if (dist >= CANT_CHUNKS) {
						/* Escribo en archivo */
						write(filefd, buffer_write, CHUNK * CANT_CHUNKS);
						total_escrito += CHUNK * CANT_CHUNKS;
//						printf("CANTIDAD ESCRITA: %d, TOTAL: %ld\n", CHUNK * CANT_CHUNKS, total_escrito);
//						printf("SALDO AL MOMENTO DE ESCRIBIR: %u\n", saldo);
//						exit(0);

						memset((void *) (buffer_sent + ETH_HEADER_LEN + RAW_HEADER_LEN), 0x0, CANT_CHUNKS);
						nro_secuencia_anterior += CANT_CHUNKS;
						if (nro_secuencia_anterior >= (0xF0 - CANT_CHUNKS)) {
							nro_secuencia_anterior -= (0xF0 - CANT_CHUNKS);
//							exit(0);
						}
						saldo = 0;
					}

					posicion = nro_secuencia % CANT_CHUNKS;

					/* Escribo en el buffer al archivo, si no fue completado aún */
//					if ((void *)(buffer_sent + ETH_HEADER_LEN + RAW_HEADER_LEN + posicion) == 0x0) {
						printf("COMPLETA POSICION %d para nro de secuencia %d\n", posicion * CHUNK, nro_secuencia);
						memcpy((void *) (buffer_write + posicion * CHUNK), (void *) (buffer_recv + ETH_HEADER_LEN + RAW_HEADER_LEN), length - ETH_HEADER_LEN - RAW_HEADER_LEN);
						memset((void *) (buffer_sent + ETH_HEADER_LEN + RAW_HEADER_LEN + posicion), 0xFF, 1);
						saldo += length - ETH_HEADER_LEN - RAW_HEADER_LEN;
//					}

					/* Termina una ráfaga. Mando ACK */
					if ((((nro_secuencia + 1) % CANT_CHUNKS) == 0) && (! primer_paquete)) {
						sent = enviar_ack(etherhead, nro_secuencia_anterior);
						if (sent == -1)
							error("enviar_ack():");
						printf("MANDO ACK para nro de secuencia %d\n", nro_secuencia);
					}

					/*
					 * La grilla en buffer de respuesta está en uno, o
					 * se leyeron menos bytes que el tamaño del CHUNK
					 * (o sea, último paquete).
					 * Mando ACK
					 */
/*
					if ((memcmp((void *) (buffer_sent + ETH_HEADER_LEN + RAW_HEADER_LEN), (void *) notnull_buffer, CANT_CHUNKS) == 0) || (length < CHUNK)) {
						sent = enviar_ack(etherhead, nro_secuencia_anterior);
						if (sent == -1)
							error("enviar_ack():");
					}
*/
/*
					if ((nuevo == (anterior + 1)) || (nuevo == 0 && anterior == 0xEF)) {
						memcpy((void *) buffer_write, (void *) (buffer_recv + ETH_HEADER_LEN + RAW_HEADER_LEN), length - ETH_HEADER_LEN - RAW_HEADER_LEN);
						write(filefd, buffer_write, length - ETH_HEADER_LEN - RAW_HEADER_LEN);

						nro_secuencia_ant = nro_secuencia;
						anterior = nuevo;

					}
*/

//					dump_buffer(buffer_recv, length);

					primer_paquete = 0;
//					printf("SALDO: %u\n", saldo);
				}
/*
				sent = enviar_ack(etherhead, nro_secuencia);
				if (sent == -1)
					error("enviar_ack():");
*/
				answered_packets++;
			} /* if (eh->h_proto == ETH_P_NULL && memcmp((const void *) eh->h_dest, (const void *) src_mac, ETH_MAC_LEN) == 0) { */

			total_packets++;


		/* Timeout en la espera por un paquete entrante */
		} else {
			//printf("Timeout para paquete entrante\n");
			if (!primer_paquete) {
				sent = enviar_ack(etherhead, nro_secuencia_anterior);
				if (sent == -1)
					error("enviar_ack():");
			}
		} /* if (retval) */

		cantidad_de_reintentos++;
		if (cantidad_de_reintentos > LIMITE_REINTENTOS) {
//			printf("Llegó al límite de reintentos\n");
			if (!primer_paquete) {
				sent = enviar_ack(etherhead, nro_secuencia_anterior);
				if (sent == -1)
					error("enviar_ack():");
			}
			cantidad_de_reintentos=0;
		}
	} /* while(1) */
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
	close(filefd);

//  free(buffer);

	printf("Server terminating....\n");

	printf("Totally received: %ld packets\n", total_packets);
	printf("Answered %ld packets\n", answered_packets);
	exit(0);
}









#if 0
void startOfRx(void)
{
    int length = 0, sent = 0;
	unsigned char nro_secuencia = 0;
	void *buffer_write = NULL;
	char path[255];

	buffer_write = (void *) malloc(BUFFER_SIZE);

	while (1) {
		/* Recupero dato leído */
		length = recvfrom(s, buffer_recv, BUFFER_SIZE, 0, NULL, NULL);
		if (length == -1) {
			printf("ERRNO: %d\n", errno);
			error("recvfrom():");
		}

		/*
		 * Compruebo si el dato recibido se corresponde a un socket raw
		 * 1) Ethertype == 0x0
		 * 2) Direccion ethernet de destino == nuestra MAC
		 * 3) Primer byte de datos == nro. de secuencia
		 */
		if (eh->h_proto == ETH_P_NULL
			&& memcmp((const void *) eh->h_dest, (const void *) src_mac,
					  ETH_MAC_LEN) == 0) {

			data = buffer_recv + ETH_HEADER_LEN;
			nro_secuencia = (unsigned char) *data;

			if (nro_secuencia == 0xFF) {
#ifdef DEBUG
    			printf("Se recibió inicio de transmisión\n");
#endif

				memcpy((void *) buffer_write, (void *) (buffer_recv + ETH_HEADER_LEN + RAW_HEADER_LEN), length - ETH_HEADER_LEN - RAW_HEADER_LEN);
				sprintf(path, "/tmp/%s", (char *)buffer_write);
				printf("Recibiendo archivo %s\n", path);
				/* Abro archivo si no ha sido abierto */
				if (filefd == 0) {
					if ((filefd = (open(path, O_WRONLY | O_CREAT | O_TRUNC))) < 0) {
						error("Error al intentar leer archivo local.\n");
					}
				}

				if ((sent = enviar_ack(etherhead, 0xFF)) == -1)
					error("enviar_ack():");

				return;
			}
		}
	}

    return;
}

#endif















int enviar_ack(unsigned char *etherhead, unsigned char nro_secuencia)
{
	int sent;

	/* La dirección destino, es la dirección origen del paquete que llegó */
	memcpy((void *) etherhead_sent, (const void *) (etherhead + ETH_MAC_LEN), ETH_MAC_LEN);

	memcpy((void *) (buffer_sent + ETH_HEADER_LEN), (const void *) &nro_secuencia, 1);

	sent = sendto(s, buffer_sent, ETH_MIN_LEN, 0,
	   (struct sockaddr *) &socket_address,
	   sizeof(socket_address));

//	dump_buffer(buffer_sent, 20);

	return sent;
}



int distancia(unsigned char nro_secuencia, unsigned char nro_secuencia_anterior)
{
	int dif = 0;
	dif = nro_secuencia - nro_secuencia_anterior;
	if (dif < 0)
		dif = 0xEF - nro_secuencia_anterior + nro_secuencia;

	return dif;
}
