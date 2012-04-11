/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2008 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
#include "ompi_config.h"
#include <stdio.h>


#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>

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

#define DEBUG

#define MTU 1500
#define IP_H_SIZE 20
#define TCP_H_SIZE 32
#define MPICH_H_SIZE 22
#define DATASIZE (MTU - IP_H_SIZE - TCP_H_SIZE - MPICH_H_SIZE)
#define DEVICE "eth0"
#define ETH_MAC_LEN ETH_ALEN
#define ETH_HEADER_LEN ETH_HLEN
#define ETH_P_NULL 0x0



#include "ompi/mpi/c/bindings.h"
#include "ompi/mca/pml/pml.h"
#include "ompi/datatype/datatype.h"
#include "ompi/memchecker.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_Mysend = PMPI_Mysend
#endif

#if OMPI_PROFILING_DEFINES
#include "ompi/mpi/c/profile/defines.h"
#endif

static const char FUNC_NAME[] = "MPI_Mysend";

//#define BUF_SIZE 1500











#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>

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

/* see linux/if_ether.h */
#define ETH_MAC_LEN        ETH_ALEN      /* Octets in one ethernet addr   */
#define ETH_HEADER_LEN     ETH_HLEN      /* Total octets in header.       */
#define ETH_MIN_FRAME_LEN  ETH_ZLEN      /* Min. octets in frame sans FCS */
#define ETH_USER_DATA_LEN  ETH_DATA_LEN  /* Max. octets in payload        */
#define ETH_MAX_FRAME_LEN  ETH_FRAME_LEN /* Max. octets in frame sans FCS */

#define ETH_FRAME_TOTALLEN 1518          /*Header: 14 + User Data: 1500 FCS: 4*/

#define DEVICE             "eth0"	/* Device used for communication*/

#define ETH_P_NULL	   0x0		/* we are running without any protocol above the Ethernet Layer*/

void print_eth_mac_addr(unsigned char* eth_header); /*prints both MAC adresses from Ethernet Header*/
void hexdump(unsigned char *buf, int nbytes);	    /*hexdumps some memory to stdout*/	
int create_udp_socket(int port);		    /*returns a socket despriptor the a newly created UDP Socket*/	

#define MTU 1500
#define IP_H_SIZE 20
#define TCP_H_SIZE 32
#define MPICH_H_SIZE 22
#define DATASIZE (MTU - IP_H_SIZE - TCP_H_SIZE - MPICH_H_SIZE)



#define BUF_SIZE ETH_FRAME_TOTALLEN
#define NUMBER_OF_MESUREMENTS_PER_AMOUNT_OF_DATA 1000
int sd = 0;						/*Socketdescriptor */
void *buffers = NULL;
void *buffer2 = NULL;
long total_sent_packets = 0;




















int MPI_Mysend(void *buf, int count, MPI_Datatype type, int dest,
			   int tag, MPI_Comm comm)
{
	int rc = MPI_SUCCESS;
//	int length = 0;
//	int s = 0, sent, ifindex = 0, i = 0;
//	struct sockaddr_ll socket_address;
//	struct ifreq ifr;

//	void *buffer = NULL;
	void *i_buff = NULL;

	buffers = (void *) malloc(BUF_SIZE);
//	unsigned char *etherhead = buffer;
//	struct ethhdr *eh = (struct ethhdr *) etherhead;
	/* Puntero a la seccion de datos de lo que se envia */
//	unsigned char *data = buffer + ETH_HEADER_LEN;

	i_buff = (void *) malloc(BUF_SIZE);
	unsigned char *i_etherhead = i_buff;
	unsigned char *i_data = i_buff + ETH_HEADER_LEN;
	struct ethhdr *i_eh = (struct ethhdr *) i_etherhead;









	buffer2 = (void *) malloc(BUF_SIZE);	/*Buffer for receiving ethernet frame */

	buffers = (void *) malloc(BUF_SIZE);	/*Buffer for ethernet frame */
	unsigned char *etherhead = buffers;	/*Pointer to ethenet header */
	unsigned char *data = buffers + 14;	/*Userdata in ethernet frame */
	struct ethhdr *eh = (struct ethhdr *) etherhead;	/*Another pointer to ethernet header */

	unsigned char src_mac[6];	/*our MAC address */
	unsigned char dest_mac[6] = {0x00, 0x19, 0xD1, 0x99, 0x79, 0xB6};

	struct ifreq ifr;
	struct sockaddr_ll socket_address;
	int ifindex = 0;			/*Ethernet Interface index */
	int i, j;
	int length;					/*length of received packet */
	int sent;					/*length of sent packet */

	/*stuff for time measuring: */
	struct timeval begin;
	struct timeval end;
	struct timeval result;
	unsigned long long allovertime;

#ifdef DEBUG
	printf("Client started, entering initialiation phase...\n");
#endif

	/*open socket */
	sd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sd == -1) {
		perror("socket():");
		exit(1);
	}
#ifdef DEBUG
	printf("Successfully opened socket: %i\n", sd);
#endif

	/*retrieve ethernet interface index */
	strncpy(ifr.ifr_name, DEVICE, IFNAMSIZ);
	if (ioctl(sd, SIOCGIFINDEX, &ifr) == -1) {
		perror("SIOCGIFINDEX");
		exit(1);
	}
	ifindex = ifr.ifr_ifindex;
#ifdef DEBUG
	printf("Successfully got interface index: %i\n", ifindex);
#endif

	/*retrieve corresponding MAC */
	if (ioctl(sd, SIOCGIFHWADDR, &ifr) == -1) {
		perror("SIOCGIFINDEX");
		exit(1);
	}
	for (i = 0; i < 6; i++) {
		src_mac[i] = ifr.ifr_hwaddr.sa_data[i];
	}
#ifdef DEBUG
	printf
		("Successfully got our MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
		 src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4],
		 src_mac[5]);
#endif

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


	/*establish signal handler */
#ifdef DEBUG
	printf("Successfully established signal handler for SIGINT\n");
#endif

	/*init random number generator */
	srand(time(NULL));

#ifdef DEBUG
	printf("We are in production state, sending packets....\n");
#endif

	/* Almacena los datos a enviar */
	for (i = 0; i < DATASIZE; i++) {
		data[i] = 'a';
	}

	/*prepare buffer */
	memcpy((void *) buffers, (void *) dest_mac, ETH_MAC_LEN);
	memcpy((void *) (buffers + ETH_MAC_LEN), (void *) src_mac,
			   ETH_MAC_LEN);
	eh->h_proto = ETH_P_NULL;

	for (i = 50; i <= DATASIZE; i += 50) {
		allovertime = 0;

		/*clear the timers: */
		timerclear(&begin);
		timerclear(&end);

		gettimeofday(&begin, NULL);

		for (j = 0; j < NUMBER_OF_MESUREMENTS_PER_AMOUNT_OF_DATA; j++) {
			/*send packet */
			sent =
				sendto(sd, buffers, i + ETH_HEADER_LEN, 0,
					   (struct sockaddr *) &socket_address,
					   sizeof(socket_address));
			if (sent == -1) {
				perror("sendto():");
				exit(1);
			}

			/*Wait for incoming packet... */
			length = recvfrom(sd, buffer2, BUF_SIZE, 0, NULL, NULL);
			printf("RECIBIDO LARGO %d\d", length);
			if (length == -1) {
				perror("recvfrom():");
				exit(1);
			}

		}
		gettimeofday(&end, NULL);
		/*...and calculate difference......... */
		timersub(&end, &begin, &result);

		allovertime += ((result.tv_sec * 1000000) + result.tv_usec);

		printf("Enviar %d bytes tarda %lld microsegundos en promedio\n", i,
			   allovertime / NUMBER_OF_MESUREMENTS_PER_AMOUNT_OF_DATA);
	}
	return(0);


































#ifdef DEBUG
	printf("Entre en MySend()\n");

/*
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	printf
		("En Mysend(), se llama Send con argumentos: BUF:%p VALOR:%d COUNT:%d TYPE:%d ORIG: %d DEST:%d TAG:%d\n",
		 buf, *(int *) buf, count, type, rank, dest, tag);
*/
#endif

/*
    rc = MCA_PML_CALL(send(buf, count, type, dest, tag, MCA_PML_BASE_SEND_STANDARD, comm));
    OMPI_ERRHANDLER_RETURN(rc, comm, rc, FUNC_NAME);
*/

	/*open socket */
	sd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sd == -1) {
		perror("socket():");
		exit(1);
	}
#ifdef DEBUG
	printf("Successfully opened socket en mysend: %i\n", sd);
#endif

	strncpy(ifr.ifr_name, DEVICE, IFNAMSIZ);
	if (ioctl(sd, SIOCGIFINDEX, &ifr) == -1) {
		perror("SIOCGIFINDEX");
		exit(1);
	}
	ifindex = ifr.ifr_ifindex;

//	unsigned char src_mac[6] = { 0x00, 0x19, 0xD1, 0x99, 0x79, 0xB6 };
//	unsigned char dest_mac[6] = { 0x00, 0x19, 0xD1, 0x99, 0x79, 0xB6 };

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
/*
	socket_address.sll_addr[0] = my_nodes->addr[0];
	socket_address.sll_addr[1] = my_nodes->addr[1];
	socket_address.sll_addr[2] = my_nodes->addr[2];
	socket_address.sll_addr[3] = my_nodes->addr[3];
	socket_address.sll_addr[4] = my_nodes->addr[4];
	socket_address.sll_addr[5] = my_nodes->addr[5];
*/
	socket_address.sll_addr[6] = 0x00;
	socket_address.sll_addr[7] = 0x00;

	/* Asigno direccion MAC destino */
	memcpy((void *) buffers, (void *) dest_mac, ETH_MAC_LEN);
	/* Asigno direccion MAC origen */
	memcpy((void *) (buffers + ETH_MAC_LEN), (void *) src_mac, ETH_MAC_LEN);
	/* Protocolo de capas superiores NULL */
	eh->h_proto = ETH_P_NULL;
	/* Copio datos */
	memcpy((void *) data, (void *) buf, count * sizeof(type));


/*
//	for (i=0; i<6; i++)
//		memcpy((void *) buffer+i, (void *) my_nodes->addr[i], 1);
//	memcpy((void *) (buffer + ETH_MAC_LEN), (void *) src_mac, ETH_MAC_LEN);
	memcpy((void *) (buffer + ETH_MAC_LEN), (void *) my_nodes->addr[0], ETH_MAC_LEN);
//	eh->h_proto = ETH_P_NULL;
*/

	/*send packet */
	printf("DUMP del paquete en send. Largo: %d, ETH_HEADER_LEN: %d\n",
		   (count * sizeof(type)) + ETH_HEADER_LEN, ETH_HEADER_LEN);
	for (i = 0; i < (count * sizeof(type) + ETH_HEADER_LEN); i++)
		printf("%02X ", *(unsigned char *) (buffers + i));
	printf("\nFin del DUMP\n");
	sent =
		sendto(sd, buffers, (count * sizeof(type)) + ETH_HEADER_LEN, 0,
			   (struct sockaddr *) &socket_address,
			   sizeof(socket_address));
	if (sent == -1) {
		perror("sendto() en send:");
		exit(1);
	}
#ifdef DEBUG
	printf("Paquete enviado sin errores\n");
#endif

	/* Espero por el handshake de respuesta */
/*
	length = recvfrom(s, buffer2, BUF_SIZE, 0, NULL, NULL);
	if (length == -1) {
		perror("recvfrom():");
		exit(1);
	}
*/


	while (length =
		   recvfrom(sd, i_buff, BUF_SIZE, 0, NULL, NULL)) {
		if (length == -1) {
			perror("recvfrom():");
			exit(1);
		}
/*
#ifdef DEBUG
		printf("Procesando un paquete recibido de largo %d\n", length);
		printf("DUMP del paquete\n");
		for (i = 0; i < ETH_HEADER_LEN + 1; i++)
			printf("%02X ", *(unsigned char *) (i_buff + i));
		printf("\nFin del DUMP\n");
#endif
*/
		/*See if we should answer (Ethertype == 0x0 && destination address == our MAC) */
		if (i_eh->h_proto == ETH_P_NULL
			&& (memcmp((const void *) i_eh->h_dest,
					  (const void *) my_mac_address, ETH_MAC_LEN) == 0) && (*i_data == 'K')) {

			printf
				("DUMPO del paquete RECIBIDO como HANDSHAKE en send. Largo: %d\n",
				 ETH_HEADER_LEN + 1);
			for (i = 0; i < length; i++)
				printf("%02X ", *(unsigned char *) (i_buff + i));
			printf("\nFin del DUMP en recv\n");
			break;

		}
	}






















/*
	if (eh->h_proto == ETH_P_NULL
            && memcmp((const void *) eh->h_dest, (const void *) src_mac,
                      ETH_MAC_LEN) == 0) {
*/
//  free(buffer);
//  free(buffer2);
	OMPI_ERRHANDLER_RETURN(rc, comm, rc, FUNC_NAME);

}
