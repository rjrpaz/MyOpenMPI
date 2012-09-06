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

#define BUF_SIZE ETH_FRAME_TOTALLEN

#define TIMEOUT 500000
//#define TIMEOUT 0
#define LIMITE_REINTENTOS 1000

//void timeout_func(int);
void sigint(int signum);

int s = 0, filefd = 0;
long total_packets = 0;
long answered_packets = 0;

int send_ack(unsigned char *etherhead, unsigned char src_mac[6], unsigned char nro_secuencia);

struct sockaddr_ll socket_address;

int main(void)
{
	/* Variable asociadas al paquete ENTRANTE */

	/* buffer que contiene paquete enviado por red */
	void *buffer_recv = NULL;
	buffer_recv = (void *) malloc(BUF_SIZE);

	/* Puntero a la cabecera ethernet del paquete RECIBIDO */
	unsigned char *etherhead = buffer_recv;

	/* Puntero a la seccion de datos en el paquete SR */
	unsigned char *data;

	/* Puntero a la estructura que almacena la información de la cabecera ethernet del paquete RECIBIDO */
	struct ethhdr *eh = (struct ethhdr *) etherhead;

	/* 6 bytes con la dirección ethernet ORIGEN en el paquete de ACK enviado */
	unsigned char src_mac[6];

	int nonblock = 1;


	/* Variables para almacenar nuestra propia dirección ethernet */
	struct ifreq ifr;
	/* Indice de la interfaz Ethernet local */
	int ifindex = 0;


	/* Variable asociadas al paquete SALIENTE */

	/* buffer que contiene paquete enviado */
	//void *buffer_sent = NULL;
	//buffer_sent = (void *) malloc(BUF_SIZE);

	/* Puntero a la cabecera ethernet del paquete ENVIADO */
	//unsigned char *etherhead_sent = buffer_sent;

	/* Puntero a la estructura que almacena la información de la cabecera ethernet del paquete ENVIADO */
	//struct ethhdr *eh_sent = (struct ethhdr *) etherhead_sent;



	// REVISAR
	int length;					/*length of received packet */
	int sent;
	char path[BUF_SIZE];
	unsigned char nro_secuencia = 0xEF, nro_secuencia_ant = 0xEF;

	void *buffer_write = NULL;
	buffer_write = (void *) malloc(BUF_SIZE);	/*Buffer for Ethernet Frame */

	unsigned short int escribir_archivo = 0, primer_paquete = 1;
	unsigned char anterior = 0xEF, nuevo = 0xEF;

	/* Variables asociada a las pruebas de retry */
	int i;
	fd_set rfds;
	struct timeval tv;
	int retval, cantidad_de_reintentos = 0;

	/* COMIENZO DEL PROGRAMA */
	printf("Arrancando servidor\n");

    /* Lanzo timer por timeout */
//    signal(SIGALRM, timeout_func);


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

	/* Defino función para salida por interrupción del programa */
	signal(SIGINT, sigint);

/*
	sprintf(path, "/tmp/server.file");
	if ((filefd = (open(path, O_WRONLY | O_CREAT | O_TRUNC))) < 0) {
		error("Error al intentar leer archivo local.\n");
	}
*/

	printf
		("Esperando paquetes entrantes\n");

	while (1) {
		/* Espero paquete entrante */
		FD_ZERO(&rfds);
		FD_SET(s, &rfds);

		/* Establece timeout */
		tv.tv_sec = 0;
		tv.tv_usec = TIMEOUT;

		retval = select(s+1, &rfds, NULL, NULL, &tv);
		//retval = select(s+1, &rfds, NULL, NULL, NULL);

		if (retval == -1)
			error("select()");
		else if (retval) {
			//printf("Se detectó paquete entrante antes del timeout\n");
			/* Recupero dato leído */
			length = recvfrom(s, buffer_recv, BUF_SIZE, 0, NULL, NULL);
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

//				printf("%u %u\n", (unsigned char) nuevo, anterior);
//         		if (((unsigned char)(nro_secuencia) == (unsigned char) (nro_secuencia_ant+1)) || (nro_secuencia == 0 && nro_secuencia_ant == 255)) {

				escribir_archivo = 0;
//				printf("Nro. secuencia ant: %u\n", (unsigned char) nro_secuencia_ant);


				/* Nombre del archivo */
				if (nuevo == 0xFF) {
					memcpy((void *) buffer_write, (void *) (buffer_recv + ETH_HEADER_LEN + RAW_HEADER_LEN), length - ETH_HEADER_LEN - RAW_HEADER_LEN);
//					printf("Se va a recibir archivo de nombre %s\n", (char *) buffer_write);
					sprintf(path, "/tmp/%s", (char *) buffer_write);
					printf("Recibiendo archivo %s\n", path);
					if ((filefd = (open(path, O_WRONLY | O_CREAT | O_TRUNC))) < 0) {
						error("Error al intentar leer archivo local.\n");
					}
					escribir_archivo = 0;
					//nro_secuencia_ant = 0xEF;
					anterior = 0xEF;
				/* EOF */
				} else if (nuevo == 0xFE) {
					close (filefd);
					printf("Archivo %s fue recibido completamente\n", path);
					primer_paquete = 0;
					escribir_archivo = 0;
				/* Contenido del archivo */
				} else {

					if ((nuevo == (anterior + 1)) || (nuevo == 0 && anterior == 0xEF)) {
						/* Escribo en archivo */
						memcpy((void *) buffer_write, (void *) (buffer_recv + ETH_HEADER_LEN + RAW_HEADER_LEN), length - ETH_HEADER_LEN - RAW_HEADER_LEN);
						write(filefd, buffer_write, length - ETH_HEADER_LEN - RAW_HEADER_LEN);
						escribir_archivo = 1;
					}

/*
#ifdef DEBUG
					printf("\nDUMP buffer_recv\n");
					for (i = 0; i < length; i++)
						printf("%02X ", *(unsigned char *) (buffer_recv + i));
					printf("\nFin de DUMP\n");
#endif
*/
					primer_paquete = 0;
				}
				sent = send_ack(etherhead, src_mac, nro_secuencia);
				if (sent == -1)
					error("send_ack():");

				if (escribir_archivo == 1) {
					nro_secuencia_ant = nro_secuencia;
					anterior = nuevo;
				}
				answered_packets++;
			} /* if (eh->h_proto == ETH_P_NULL && memcmp((const void *) eh->h_dest, (const void *) src_mac, ETH_MAC_LEN) == 0) { */

			total_packets++;


		/* Timeout en la espera por un paquete entrante */
		} else {
			//printf("Timeout para paquete entrante\n");
			if (!primer_paquete) {
				sent = send_ack(etherhead, src_mac, nro_secuencia);
				if (sent == -1)
					error("send_ack():");
			}
		} /* if (retval) */

		cantidad_de_reintentos++;
		if (cantidad_de_reintentos > LIMITE_REINTENTOS) {
//			printf("Llegó al límite de reintentos\n");
			if (!primer_paquete) {
				sent = send_ack(etherhead, src_mac, nro_secuencia);
				if (sent == -1)
					error("send_ack():");
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
void timeout_func(int signo)
{
	int sent = 0;
    printf("Llego SIGARLM\n");
/*
    timeout_count++;
    printf("Llego SIGARLM. timeout_count %ld, filefd %d, total_file_read %ld\n", timeout_count, filefd, total_file_read);
    if (lseek(filefd, total_file_read, SEEK_SET) < 0) {
        printf("No se pudo reposicionar archivo\n");
        exit(0);
    }
*/
	// Reenvío el ACK
	sent = sendto(s, buffer_sent, ETH_HEADER_LEN + RAW_HEADER_LEN, 0, (struct sockaddr *) &socket_address, sizeof(socket_address));
	if (sent == -1) {
		perror("sendto():");
		exit(1);
	}

    return;
}

*/
#endif



int send_ack(unsigned char *etherhead, unsigned char src_mac[6], unsigned char nro_secuencia) {
	int sent;

//	printf("Enviado ACK para nro. de secuencia %u\n", nro_secuencia);

	/* buffer que contiene paquete enviado */
	void *buffer_sent = NULL;
	buffer_sent = (void *) malloc(BUF_SIZE);

	/* Puntero a la cabecera ethernet del paquete ENVIADO */
	unsigned char *etherhead_sent = buffer_sent;

	/* Puntero a la estructura que almacena la información de la cabecera ethernet del paquete ENVIADO */
	struct ethhdr *eh_sent = (struct ethhdr *) etherhead_sent;

	/* intercambio direcciones MAC origen y destinos para el paquete de ACK */
	memcpy((void *) etherhead_sent, (const void *) (etherhead + ETH_MAC_LEN), ETH_MAC_LEN);
	memcpy((void *) (etherhead_sent + ETH_MAC_LEN), (const void *) src_mac, ETH_MAC_LEN);

	memcpy((void *) (buffer_sent + ETH_HEADER_LEN), (const void *) &nro_secuencia, 1);

	// DEBUGGING
	char serverID = 'S';
	memcpy((void *) (buffer_sent + ETH_HEADER_LEN + 1), (const void *) &serverID, 1);

	/*prepare sockaddr_ll */
	socket_address.sll_addr[0] = eh_sent->h_dest[0];
	socket_address.sll_addr[1] = eh_sent->h_dest[1];
	socket_address.sll_addr[2] = eh_sent->h_dest[2];
	socket_address.sll_addr[3] = eh_sent->h_dest[3];
	socket_address.sll_addr[4] = eh_sent->h_dest[4];
	socket_address.sll_addr[5] = eh_sent->h_dest[5];

//	sent = sendto(s, buffer, length, 0, (struct sockaddr*)&socket_address, sizeof(socket_address));
	sent = sendto(s, buffer_sent, ETH_HEADER_LEN + RAW_HEADER_LEN, 0,
	   (struct sockaddr *) &socket_address,
	   sizeof(socket_address));


#ifdef DEBUG
	printf("\nDUMP buffer_sent\n");
	for (i = 0; i < 20; i++)
		printf("%02X ", *(unsigned char *) (buffer_sent + i));
	printf("\nFin de DUMP\n");
#endif

	return sent;
}
