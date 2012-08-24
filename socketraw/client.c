//#include "client.h"
#include "ethertest.h"
//#include "util.h"
//#include "functions.h"

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
#include <time.h>

//#define DEBUG 1
#undef DEBUG

#define CHUNK 1024

#define RETRY 3

#define BUF_SIZE ETH_FRAME_TOTALLEN
// #define NUMBER_OF_MESUREMENTS_PER_AMOUNT_OF_DATA 100000 /*how often to measure travelling time with one certain amount of data*/
#define NUMBER_OF_MESUREMENTS_PER_AMOUNT_OF_DATA 1000	/*how often to measure travelling time with one certain amount of data */

void sigint(int);

void usage(char *);

extern void error(char *);

//extern void timeout_func(int);
void timeout_func(int);


int s = 0;						/*Socketdescriptor */

//void *buffer = NULL;
long total_sent_packets = 0;
unsigned long total_file_read = 0;

unsigned long timeout_count = 0;
int filefd = 0;					/* Descriptor del archivo que se va a transferir */

int main(int argc, char *argv[])
{
	void *buffer_sent = NULL;	/* buffer que contiene paquete enviado por red */
	buffer_sent = (void *) malloc(BUF_SIZE);

	unsigned char *etherhead = buffer_sent;	/*Pointer to ethenet header */
	unsigned char *data = buffer_sent + 14;	/*Userdata in ethernet frame */
	struct ethhdr *eh = (struct ethhdr *) etherhead;	/*Another pointer to ethernet header */

	unsigned char src_mac[6];	/*our MAC address */
//  unsigned char dest_mac[6] = {0x00, 0x04, 0x75, 0xC8, 0x28, 0xE5};   /*other host MAC address, hardcoded...... :-(*/
	// Consulfem
	unsigned char dest_mac[6] = { 0x00, 0x19, 0xD1, 0x99, 0x79, 0xB6 };
	// Casa wlan0
	// unsigned char dest_mac[6] = { 0x00, 0x22, 0x43, 0x0d, 0x5c, 0x4b };

	struct ifreq ifr;
	struct sockaddr_ll socket_address;
	int ifindex = 0;			/*Ethernet Interface index */
	int i;

	void *buffer_read = NULL;	/* buffer que contiene chunk de archivo leido */
	int length_read = 0;
	int length_sent = 0;		/* largo de paquete enviado */

	void *buffer_recv = NULL;	/* buffer que contiene paquete leido */
	buffer_recv = (void *) malloc(BUF_SIZE);
	unsigned char *etherhead_recv = buffer_recv;	/*Pointer to ethenet header */
	struct ethhdr *eh_recv = (struct ethhdr *) etherhead_recv;	/*Another pointer to ethernet header */

	int length_recv = 0;		/* largo de paquete recibido */
	char host[BUF_SIZE], path[BUF_SIZE], filename[BUF_SIZE];
	char *parse;

	/*stuff for time measuring: */
	struct timeval begin;
	struct timeval end;
	struct timeval result;
	unsigned long long allovertime;

	unsigned char nro_secuencia = 0;

	buffer_read = (void *) malloc(BUF_SIZE);

	if (argc != 3)
		usage(argv[0]);

	sprintf(host, "%s", argv[1]);
	sprintf(path, "%s", argv[2]);

	int nro_retry_congelado = 0;

	parse = malloc(BUF_SIZE);
	sprintf(parse, "%s", path);
	parse = strtok(parse, "/");

	while (parse != NULL) {
		sprintf(filename, "%s", parse);
		parse = strtok(NULL, "/");
	}

	printf("Client started, entering initialiation phase...\n");

	/* Lanzo timer por timeout */
	signal(SIGALRM, timeout_func);

	/*open socket */
	s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (s == -1) {
		perror("socket():");
		exit(1);
	}
#ifdef DEBUG
	printf("Successfully opened socket: %i\n", s);
#endif

	/*retrieve ethernet interface index */
	strncpy(ifr.ifr_name, DEVICE, IFNAMSIZ);
	if (ioctl(s, SIOCGIFINDEX, &ifr) == -1) {
		perror("SIOCGIFINDEX");
		exit(1);
	}
	ifindex = ifr.ifr_ifindex;
#ifdef DEBUG
	printf("Successfully got interface index: %i\n", ifindex);
#endif

	/*retrieve corresponding MAC */
	if (ioctl(s, SIOCGIFHWADDR, &ifr) == -1) {
		perror("SIOCGIFINDEX");
		exit(1);
	}
	for (i = 0; i < 6; i++) {
		src_mac[i] = ifr.ifr_hwaddr.sa_data[i];
	}
	printf
		("Successfully got our MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
		 src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4],
		 src_mac[5]);

	/*prepare sockaddr_ll */
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

	/*establish signal handler */
	signal(SIGINT, sigint);
#ifdef DEBUG
	printf("Successfully established signal handler for SIGINT\n");

	printf("We are in production state, sending packets....\n");
#endif

	allovertime = 0;

//  printf("PATH: %s\n", path);
	/* Abro archivo */
	if ((filefd = (open(path, O_RDONLY))) < 0) {
		error("Error al intentar leer archivo local.\n");
	}
//  printf("FILEFD: %d\n", filefd);

	nro_retry_congelado = 0;
	memset(buffer_read, 0x0, BUF_SIZE);
	memset(buffer_sent, 0x0, BUF_SIZE);
	/* Leo archivo de forma secuencial */
	while ((length_read = read(filefd, buffer_read, CHUNK)) > 0) {
#ifdef DEBUG
		printf("Se leyeron %d bytes del archivo\n", length_read);
#endif
		total_file_read += length_read;

		if (length_read != CHUNK)
			printf("Se quisieron leer %d pero se leyeron %d bytes del archivo\n", CHUNK, length_read);
		/* Lanzo timer por timeout */
//		printf("Refresco alarma\n");
		alarm(3);


		// El programa se "clava" 
		if ((total_sent_packets % 700 == 0) && (total_sent_packets > 0)) {
//			printf("Nro_retry_congelado %d\n", nro_retry_congelado);
			if (nro_retry_congelado == 0) {
				printf("El programa se congela\n");
				nro_retry_congelado = 1;
				pause();
				//for(;;);
			} else {
				nro_retry_congelado = 0;
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
		for (i = 0; i < 14; i++)
			printf("%02X ", *(unsigned char *) (buffer_sent + i));
		printf("\nFin de DUMP\n");
#endif

//      eh->h_proto = ETH_P_NULL;
		/* Numero de secuencia */
		data[0] = nro_secuencia;
		/* Contenido del chunk del archivo */
//      printf("Marca LENGTH: %d\n", length_read);
		memcpy((void *) (data + 1), (void *) buffer_read, length_read);
//      printf("BufferSENT: %s\n", (char *) buffer_sent);


#ifdef DEBUG
		printf("\nDUMP datos leidos archivo\n");
		for (i = 0; i < length_read + 2; i++)
			printf("%02X ", *(unsigned char *) (data + i));
		printf("\nFin de DUMP\n");

		printf("\nDUMP buffer_sent\n");
		for (i = 0; i < length_read + 14 + 2; i++)
			printf("%02X ", *(unsigned char *) (buffer_sent + i));
		printf("\nFin de DUMP\n");
#endif

//resend:

		/*clear the timers: */
		timerclear(&begin);
		timerclear(&end);

		/*get time before sending..... */
		gettimeofday(&begin, NULL);


		/* Envio paquete con pedazo de archivo */
		length_sent =
			sendto(s, buffer_sent, ETH_HEADER_LEN + length_read + 1, 0,
				   (struct sockaddr *) &socket_address,
				   sizeof(socket_address));
		if (length_sent < 0) {
			perror("sendto():");
			exit(1);
		}
//      printf("Enviado\n");

		/* Espero por handshake de pedazo enviado */
		while ((length_recv =
				recvfrom(s, buffer_recv, BUF_SIZE, 0, NULL, NULL)) != -1) {

			/*
			 * Espero para recibir el paquete de ACK, que debe incluir:
			 * 1) Ethertype == 0x0
			 * 2) Direccion ethernet de destino == nuestra MAC
			 * 3) Primer byte de datos == nro. de secuencia
			 */
			if (eh_recv->h_proto == ETH_P_NULL && (length_recv == 15)
				&&
				(memcmp
				 ((const void *) eh_recv->h_dest, (const void *) src_mac,
				  ETH_MAC_LEN) == 0)
				&&
				(memcmp
				 ((unsigned char *) (buffer_recv + 14),
				  (const void *) &nro_secuencia, 1) == 0)
				) {


//printf("Reseteo alarma\n");
alarm(0);

//#ifdef DEBUG
				unsigned char *data_recv = buffer_recv + 14;	/*Userdata in ethernet frame */
/*
				printf
					("Nro. secuencia local: %u. Nro. secuencia ACK: %d. Largo: %d\n",
					 nro_secuencia, (unsigned int) *(data_recv),
					 length_recv);
*/

#ifdef DEBUG
				printf("\nDUMP handshake received\n");
				for (i = 0; i < 15; i++)
					printf("%02X ", *(unsigned char *) (buffer_recv + i));
				printf("\nFin de DUMP\n");
#endif

				/* Chequea si el nro. de secuencia es igual al anterior */


				/*get time after sending..... */
				gettimeofday(&end, NULL);
				/*...and calculate difference......... */
				timersub(&end, &begin, &result);

				allovertime +=
					((result.tv_sec * 1000000) + result.tv_usec);

				nro_secuencia++;
				break;
			}
		}

		total_sent_packets++;

		if ((total_file_read % CHUNK) != 0)
			printf("Tamaño leído del archivo es %ld, que no es múltiplo de %d\n", total_file_read, CHUNK);
	

		memset(buffer_read, 0x0, BUF_SIZE);
		memset(buffer_sent, 0x0, BUF_SIZE);
		memset(buffer_recv, 0x0, BUF_SIZE);
	} //while ((length_read = read(filefd, buffer_read, CHUNK)) > 0) {

//  printf("Sending %i bytes takes %lld microseconds in average\n", i, allovertime / NUMBER_OF_MESUREMENTS_PER_AMOUNT_OF_DATA);

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

