#include "server.h"
#include "util.h"

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

//#define DEBUG 1
#undef DEBUG

#define BUF_SIZE ETH_FRAME_TOTALLEN

int s = 0, filefd = 0;			/*Socketdescriptor */
//void *buffer = NULL;
long total_packets = 0;
long answered_packets = 0;

void error(char *);

int main(void)
{
	void *buffer_recv = NULL;
	buffer_recv = (void *) malloc(BUF_SIZE);	/*Buffer for Ethernet Frame */
	unsigned char *etherhead = buffer_recv;	/*Pointer to Ethenet Header */
	struct ethhdr *eh = (struct ethhdr *) etherhead;	/*Another pointer to ethernet header */

	unsigned char src_mac[6];	/*our MAC address */


	void *buffer_sent = NULL;
	buffer_sent = (void *) malloc(BUF_SIZE);	/*Buffer for Ethernet Frame */
	unsigned char *etherhead_sent = buffer_sent;	/*Pointer to Ethenet Header */
	struct ethhdr *eh_sent = (struct ethhdr *) etherhead_sent;	/*Another pointer to ethernet header */

	struct ifreq ifr;
	struct sockaddr_ll socket_address;
	int ifindex = 0;			/*Ethernet Interface index */
	int i;
	int length;					/*length of received packet */
	int sent;
	char path[BUF_SIZE];

	void *buffer_write = NULL;
	buffer_write = (void *) malloc(BUF_SIZE);	/*Buffer for Ethernet Frame */

	printf("Server started, entering initialiation phase...\n");

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
	socket_address.sll_addr[6] = 0x00;
	socket_address.sll_addr[7] = 0x00;


	/*establish signal handler */
	signal(SIGINT, sigint);
#ifdef DEBUG
	printf("Successfully established signal handler for SIGINT\n");
#endif

	sprintf(path, "/tmp/server.file");
	if ((filefd = (open(path, O_WRONLY | O_CREAT | O_TRUNC))) < 0) {
		error("Error al intentar leer archivo local.\n");
	}

#ifdef DEBUG
	printf
		("We are in production state, waiting for incoming packets....\n");
#endif

	while (1) {
		/*Wait for incoming packet... */
		length = recvfrom(s, buffer_recv, BUF_SIZE, 0, NULL, NULL);
		if (length == -1) {
			perror("recvfrom():");
			exit(1);
		}

		/*See if we should answer (Ethertype == 0x0 && destination address == our MAC) */
		if (eh->h_proto == ETH_P_NULL
			&& memcmp((const void *) eh->h_dest, (const void *) src_mac,
					  ETH_MAC_LEN) == 0) {

//#ifdef DEBUG
			unsigned char *data = buffer_recv + 14;
			printf("Nro. secuencia: %d. Largo: %d\n",
				   (unsigned int) *(data), length);
#ifdef DEBUG
			printf("\nDUMP buffer_recv\n");
			for (i = 0; i < length; i++)
				printf("%02X ", *(unsigned char *) (buffer_recv + i));
			printf("\nFin de DUMP\n");
#endif

			/*exchange addresses in buffer */
			memcpy((void *) etherhead_sent,
				   (const void *) (etherhead + ETH_MAC_LEN), ETH_MAC_LEN);
			memcpy((void *) (etherhead_sent + ETH_MAC_LEN),
				   (const void *) src_mac, ETH_MAC_LEN);

			/* Copio ACK */
			memcpy((void *) (buffer_sent + 14),
				   (const void *) (buffer_recv + 14), 1);

			/*prepare sockaddr_ll */
			socket_address.sll_addr[0] = eh_sent->h_dest[0];
			socket_address.sll_addr[1] = eh_sent->h_dest[1];
			socket_address.sll_addr[2] = eh_sent->h_dest[2];
			socket_address.sll_addr[3] = eh_sent->h_dest[3];
			socket_address.sll_addr[4] = eh_sent->h_dest[4];
			socket_address.sll_addr[5] = eh_sent->h_dest[5];

			/* ACK, devuelve nro. de secuencia */
//          sent = sendto(s, buffer, length, 0, (struct sockaddr*)&socket_address, sizeof(socket_address));
			sent =
				sendto(s, buffer_sent, 15, 0,
					   (struct sockaddr *) &socket_address,
					   sizeof(socket_address));
			if (sent == -1) {
				perror("sendto():");
				exit(1);
			}

//#ifdef DEBUG
			printf("\nDUMP buffer_sent\n");
			for (i = 0; i < 20; i++)
				printf("%02X ", *(unsigned char *) (buffer_sent + i));
			printf("\nFin de DUMP\n");
//#endif

			memcpy((void *) buffer_write, (void *) (buffer_recv + 15),
				   length - 15);
			write(filefd, buffer_write, length - 15);
//			sync();

			answered_packets++;
		}

		total_packets++;
	}
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

void error(char *msg)
{
	if (errno == 0) {
		fprintf(stderr, msg);
	} else {
		perror(msg);
	}
	exit(0);
}