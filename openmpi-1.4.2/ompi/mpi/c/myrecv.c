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

#include "ompi/mpi/c/bindings.h"
#include "ompi/mca/pml/pml.h"
#include "ompi/memchecker.h"
#include "ompi/request/request.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_Myrecv = PMPI_Myrecv
#endif

#if OMPI_PROFILING_DEFINES
#include "ompi/mpi/c/profile/defines.h"
#endif

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

#define ANSWER_OK 'K'

#define MTU 1500
#define IP_H_SIZE 20
#define TCP_H_SIZE 32
#define MPICH_H_SIZE 22
#define DATASIZE (MTU - IP_H_SIZE - TCP_H_SIZE - MPICH_H_SIZE)
#define DEVICE "eth0"
#define ETH_MAC_LEN ETH_ALEN	/* Octets in one ethernet addr   */
#define ETH_HEADER_LEN ETH_HLEN
//#define BUF_SIZE 1500
#define ETH_MIN_FRAME_LEN  ETH_ZLEN	/* Min. octets in frame sans FCS */
#define ETH_USER_DATA_LEN  ETH_DATA_LEN	/* Max. octets in payload        */
#define ETH_MAX_FRAME_LEN  ETH_FRAME_LEN	/* Max. octets in frame sans FCS */
#define ETH_FRAME_TOTALLEN 1518	/*Header: 14 + User Data: 1500 FCS: 4 */
#define ETH_P_NULL         0x0	/* we are running without any protocol above the Ethernet Layer */


















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


#define ETH_MAC_LEN        ETH_ALEN      /* Octets in one ethernet addr   */
#define ETH_HEADER_LEN     ETH_HLEN      /* Total octets in header.       */
#define ETH_MIN_FRAME_LEN  ETH_ZLEN      /* Min. octets in frame sans FCS */
#define ETH_USER_DATA_LEN  ETH_DATA_LEN  /* Max. octets in payload        */
#define ETH_MAX_FRAME_LEN  ETH_FRAME_LEN /* Max. octets in frame sans FCS */

#define ETH_FRAME_TOTALLEN 1518          /*Header: 14 + User Data: 1500 FCS: 4*/

#define DEVICE             "eth0"	/* Device used for communication*/

#define ETH_P_NULL	   0x0		/* we are running without any protocol above the Ethernet Layer*/

void sigint(int signum); 

void print_eth_mac_addr(unsigned char* eth_header); /*prints both MAC adresses from Ethernet Header*/
void hexdump(unsigned char *buf, int nbytes);	    /*hexdumps some memory to stdout*/	
int create_udp_socket(int port);		    /*returns a socket despriptor the a newly created UDP Socket*/	

#define MTU 1500
#define IP_H_SIZE 20
#define TCP_H_SIZE 32
#define MPICH_H_SIZE 22
#define DATASIZE (MTU - IP_H_SIZE - TCP_H_SIZE - MPICH_H_SIZE)


#define BUF_SIZE ETH_FRAME_TOTALLEN

//int s = 0;						/*Socketdescriptor */
void *buffer = NULL;
long total_packets = 0;
long answered_packets = 0;

























static const char FUNC_NAME[] = "MPI_Myrecv";


int MPI_Myrecv(void *buf, int count, MPI_Datatype type, int source,
			   int tag, MPI_Comm comm, MPI_Status * status)
{
	int rc = MPI_SUCCESS;
//	int length = 0;
//	int sent, i = 0, s = 0;

//	void *buffer = NULL;
	void *o_buff = NULL;

	buffer = (void *) malloc(BUF_SIZE);	/*Buffer for Ethernet Frame */
//	unsigned char *etherhead = buffer;	/*Pointer to Ethenet Header */
//	struct ethhdr *eh = (struct ethhdr *) etherhead;	/*Another pointer to ethernet header */
	unsigned char *data = o_buff + ETH_HEADER_LEN;

	o_buff = (void *) malloc(ETH_HEADER_LEN + 1);	/*Buffer for Ethernet Frame */
	unsigned char *o_etherhead = o_buff;
	struct ethhdr *o_eh = (struct ethhdr *) o_etherhead;
	unsigned char *o_data = o_buff + ETH_HEADER_LEN;




















	buffer = (void *) malloc(BUF_SIZE);	/*Buffer for Ethernet Frame */
	unsigned char *etherhead = buffer;	/*Pointer to Ethenet Header */
	struct ethhdr *eh = (struct ethhdr *) etherhead;	/*Another pointer to ethernet header */

	unsigned char src_mac[6];	/*our MAC address */

	struct ifreq ifr;
	struct sockaddr_ll socket_address;
	int ifindex = 0;			/*Ethernet Interface index */
	int i;
	int length;					/*length of received packet */
	int sent;

	printf("Server started, entering initialiation phase...\n");

//	close(my_socket_raw_send);
	/*open socket */
/*
	s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (s == -1) {
		perror("socket():");
		exit(1);
	}
	printf("Successfully opened socket: %i\n", s);
*/
#ifdef DEBUG
//  printf("Successfully opened socket: %i\n", sd);
    printf("Successfully opened socket en myrecv: %i\n", my_socket_raw_recv);
#endif

	/*retrieve ethernet interface index */
	strncpy(ifr.ifr_name, DEVICE, IFNAMSIZ);
//	if (ioctl(s, SIOCGIFINDEX, &ifr) == -1) {
	if (ioctl(my_socket_raw_recv, SIOCGIFINDEX, &ifr) == -1) {
		perror("SIOCGIFINDEX en myrecv");
		exit(1);
	}
	ifindex = ifr.ifr_ifindex;
	printf("Successfully got interface index: %i\n", ifindex);

	/*retrieve corresponding MAC */
//	if (ioctl(s, SIOCGIFHWADDR, &ifr) == -1) {
	if (ioctl(my_socket_raw_recv, SIOCGIFHWADDR, &ifr) == -1) {
		perror("SIOCGIFHWADDR en myrecv");
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
	socket_address.sll_addr[6] = 0x00;
	socket_address.sll_addr[7] = 0x00;


	/*establish signal handler */
	printf("Successfully established signal handler for SIGINT\n");

	printf
		("We are in production state, waiting for incoming packets....\n");

	while (1) {
		/*Wait for incoming packet... */
		memset(buffer, 0x0, BUF_SIZE);
//		length = recvfrom(s, buffer, BUF_SIZE, 0, NULL, NULL);
		length = recvfrom(my_socket_raw_recv, buffer, BUF_SIZE, 0, NULL, NULL);
		if (length == -1) {
			perror("recvfrom():");
			exit(1);
		}

		/*See if we should answer (Ethertype == 0x0 && destination address == our MAC) */
		if (eh->h_proto == ETH_P_NULL
			&& memcmp((const void *) eh->h_dest, (const void *) src_mac,
					  ETH_MAC_LEN) == 0) {
			printf("LARGO: %d ETH_MAC_LEN: %d\n", length, ETH_MAC_LEN);
			if (length < 5) {
				printf("LARGO: %d ETH_MAC_LEN: %d\n", length, ETH_MAC_LEN);

				for (i = 0; i < ETH_MAC_LEN; i++)
					printf("%02X ", *(src_mac + i));

				printf("\tDUMP del paquete\n\t");
				for (i = 0; i < length; i++)
					printf("%02X ", *(unsigned char *) (buffer + i));
				printf("\n\tFin del DUMP\n");
			}
			if (length == ETH_HEADER_LEN)
				break;

			/*exchange addresses in buffer */
			memcpy((void *) etherhead,
				   (const void *) (etherhead + ETH_MAC_LEN), ETH_MAC_LEN);
			memcpy((void *) (etherhead + ETH_MAC_LEN),
				   (const void *) src_mac, ETH_MAC_LEN);

			/*prepare sockaddr_ll */
			socket_address.sll_addr[0] = eh->h_dest[0];
			socket_address.sll_addr[1] = eh->h_dest[1];
			socket_address.sll_addr[2] = eh->h_dest[2];
			socket_address.sll_addr[3] = eh->h_dest[3];
			socket_address.sll_addr[4] = eh->h_dest[4];
			socket_address.sll_addr[5] = eh->h_dest[5];

			/*send answer */
/*
			sent =
				sendto(s, buffer, length - 4, 0,
					   (struct sockaddr *) &socket_address,
					   sizeof(socket_address));
*/
/*
			sent =
				sendto(my_socket_raw_recv, buffer, length - 4, 0,
					   (struct sockaddr *) &socket_address,
					   sizeof(socket_address));
			if (sent == -1) {
				perror("sendto en MPI_Myrecv ():");
				printf("Largo: %d\n", length - 4);
				exit(1);
			}

*/
			answered_packets++;
			if (answered_packets % 1000 == 0)
				printf("Fueron respondidos %d datagramas\n", answered_packets);

		}

		total_packets++;
	}
	printf("Volviendo de MPI_MyRecv ...\n");
	printf("Total de paquetes procesados: %ld packets\n", answered_packets);
	OMPI_ERRHANDLER_RETURN(rc, comm, rc, FUNC_NAME);









	// BORRAR
	int s = 0;
















#ifdef DEBUG
//	printf("\tEntre en MyRecv() (rank %d)\n", my_rank);


	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	printf("\tEn MyRecv(), se llama Recv con argumentos: BUF:%p COUNT:%d TYPE:%d RANK: %d SOURCE:%d TAG:%d\n", buf, count, type, rank, source, tag);

#endif

/*
    rc = MCA_PML_CALL(recv(buf, count, type, source, tag, comm, status));
    OMPI_ERRHANDLER_RETURN(rc, comm, rc, FUNC_NAME);
*/

//printf("Server started, entering initialiation phase...\n");

	/*
	 * Abre socket nuevamente en esta función.
	 * Si mantengo el socket abierto desde la funcion "my_init",
	 * dentro de esta función hay una relectura del mismo datagrama,
	 * como si no se vaciara el buffer.
	 */
//	my_socket_raw = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	s = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
//	if (my_socket_raw == -1) {
	if (s == -1) {
		perror("socket():");
		exit(1);
	}

//  printf("Successfully opened socket en myrecv: %i\n", s);

//  printf("Successfully got interface index en myrecv: %i\n", my_ifindex);
//  printf ("MI MAC address ES: %02X:%02X:%02X:%02X:%02X:%02X\n", my_mac_address[0], my_mac_address[1], my_mac_address[2], my_mac_address[3], my_mac_address[4], my_mac_address[5]);

  printf ("Marca 1\n");
//  memcpy((void *) data, (const void *) "K", 1);

	*o_data = 'K';
	o_eh->h_proto = ETH_P_NULL;





	/*prepare sockaddr_ll */
//	struct ifreq ifr;
//	int ifindex = 0;

	strncpy(ifr.ifr_name, DEVICE, IFNAMSIZ);
//	if (ioctl(my_socket_raw, SIOCGIFINDEX, &ifr) == -1) {
	if (ioctl(s, SIOCGIFINDEX, &ifr) == -1) {
		perror("SIOCGIFINDEX");
		exit(1);
	}
	ifindex = ifr.ifr_ifindex;

	unsigned char dest_mac[6] = { 0x00, 0x19, 0xD1, 0x99, 0x79, 0xB6 };
	my_socket_address.sll_family = PF_PACKET;
	my_socket_address.sll_protocol = htons(ETH_P_IP);
	my_socket_address.sll_ifindex = ifindex;
	my_socket_address.sll_hatype = ARPHRD_ETHER;
	my_socket_address.sll_pkttype = PACKET_OTHERHOST;
	my_socket_address.sll_halen = ETH_ALEN;
	my_socket_address.sll_addr[0] = dest_mac[0];
	my_socket_address.sll_addr[1] = dest_mac[1];
	my_socket_address.sll_addr[2] = dest_mac[2];
	my_socket_address.sll_addr[3] = dest_mac[3];
	my_socket_address.sll_addr[4] = dest_mac[4];
	my_socket_address.sll_addr[5] = dest_mac[5];















  printf ("Marca 2\n");

	while (1) {
		/*Wait for incoming packet... */
		length = recvfrom(s, buffer, BUF_SIZE, 0, NULL, NULL);
//		   recvfrom(my_socket_raw, buffer, BUF_SIZE, 0, NULL, NULL)) {
		if (length == -1) {
			perror("recvfrom():");
			exit(1);
		}

#ifdef DEBUG
		printf("\tProcesando un paquete recibido de largo %d\n", length);
		printf("\tDUMP del paquete\n\t");
		for (i = 0; i < length; i++)
			printf("%02X ", *(unsigned char *) (buffer + i));
		printf("\n\tFin del DUMP\n");
#endif

		/*See if we should answer (Ethertype == 0x0 && destination address == our MAC) */
		if (eh->h_proto == ETH_P_NULL
			&& memcmp((const void *) eh->h_dest,
					  (const void *) my_mac_address, ETH_MAC_LEN) == 0) {
			printf("\tDUMPO del paquete en recv. Largo: %d\n\t", length);
			for (i = 0; i < length; i++)
				printf("%02X ", *(unsigned char *) (buffer + i));
			printf("\n\tFin del DUMP en recv\n");

			/*exchange addresses in buffer */
			memcpy((void *) o_buff,
				   (const void *) (etherhead + ETH_MAC_LEN), ETH_MAC_LEN);
			memcpy((void *) (o_buff + ETH_MAC_LEN),
				   (const void *) my_mac_address, ETH_MAC_LEN);
			/* Nro. de secuencia */
			memcpy((void *) (o_buff + ETH_HEADER_LEN),
				   (const void *) (buffer + ETH_HEADER_LEN), 1);

/*
			my_socket_address.sll_addr[0] = eh->h_dest[0];
			my_socket_address.sll_addr[1] = eh->h_dest[1];
			my_socket_address.sll_addr[2] = eh->h_dest[2];
			my_socket_address.sll_addr[3] = eh->h_dest[3];
			my_socket_address.sll_addr[4] = eh->h_dest[4];
			my_socket_address.sll_addr[5] = eh->h_dest[5];
*/

/*
    printf("DUMP de la repuesta en recv\n");
    for (i=0; i<length; i++)
        printf("%02X ", *(unsigned char *)(o_buff + i));
    printf("\nFin del DUMP en recv\n");
*/
			/*send answer */
/*
			sent =
				sendto(s, o_buff, ETH_HEADER_LEN + 1, 0,
					   (struct sockaddr *) &my_socket_address,
					   sizeof(my_socket_address));
			if (sent == -1) {
				perror("sendto() en recv:");
				exit(1);
			}
*/
			memcpy((void *) (buf), (const void *) buffer + ETH_HEADER_LEN,
				   length - ETH_HEADER_LEN);

			printf
				("\tVALOR INTERNO EN RECV: %d en direccion %p, habiendo copiado %d bytes\n",
				 *(unsigned char *) buf, buf, length - ETH_HEADER_LEN);
//        printf("%02X ", *(unsigned char *)(buffer + i));
//          printf("YA PUEDO VOLVER de RECV\n");

//          free(buffer);
//          close(my_socket_raw);


			printf
				("\tDUMPO del paquete en DE RESPUESTA EN recv. Largo: %d\n\t",
				 ETH_HEADER_LEN + 1);
			for (i = 0; i < ETH_HEADER_LEN + 1; i++)
				printf("%02X ", *(unsigned char *) (o_buff + i));
			printf("\n\tFin del DUMP en recv\n");

			/*send answer */
//			sent = sendto(my_socket_raw, o_buff, ETH_HEADER_LEN + 1, 0, (struct sockaddr *) &my_socket_address, sizeof(my_socket_address));
			sent = sendto(s, o_buff, ETH_HEADER_LEN + 1, 0, (struct sockaddr *) &my_socket_address, sizeof(my_socket_address));
			if (sent == -1) {
				perror("sendto() en recv:");
				exit(1);
			}
			printf ("\tNO HUBO PROBLEMAS EN ENVIAR HANDSHAKE\n");






//          free(buffer);
//          close(my_socket_raw);






			OMPI_ERRHANDLER_RETURN(rc, comm, rc, FUNC_NAME);
			printf("\tESTO YA NO SE IMPRIME\n");
		}
	}
//  free (buffer);
//  free (o_buff);
//	close(my_socket_raw);

	close(s);

	OMPI_ERRHANDLER_RETURN(rc, comm, rc, FUNC_NAME);

}
