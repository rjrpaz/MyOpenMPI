//#include "client.h"
//#include "util.h"

//#include <sys/types.h>
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

#include "client.h"
#include "util.h"


#define BUF_SIZE ETH_FRAME_TOTALLEN
#define NUMBER_OF_MESUREMENTS_PER_AMOUNT_OF_DATA 1000000	/*how often to measure travelling time with one certain amount of data */

int s = 0;						/*Socketdescriptor */
void *buffer = NULL;
void *buffer2 = NULL;
long total_sent_packets = 0;

int main(void)
{

	buffer2 = (void *) malloc(BUF_SIZE);	/*Buffer for receiving ethernet frame */

	buffer = (void *) malloc(BUF_SIZE);	/*Buffer for ethernet frame */
	unsigned char *etherhead = buffer;	/*Pointer to ethenet header */
	unsigned char *data = buffer + 14;	/*Userdata in ethernet frame */
	struct ethhdr *eh = (struct ethhdr *) etherhead;	/*Another pointer to ethernet header */

	unsigned char src_mac[6];	/*our MAC address */
//	unsigned char dest_mac[6] = { 0x00, 0xE0, 0x81, 0x5C, 0x38, 0xDE };	/*other host MAC address, hardcoded...... :-( */
	unsigned char dest_mac[6] = {0x00, 0x1B, 0x24, 0x3D, 0xDA, 0xEF};

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
	socket_address.sll_addr[0] = dest_mac[0];
	socket_address.sll_addr[1] = dest_mac[1];
	socket_address.sll_addr[2] = dest_mac[2];
	socket_address.sll_addr[3] = dest_mac[3];
	socket_address.sll_addr[4] = dest_mac[4];
	socket_address.sll_addr[5] = dest_mac[5];
	socket_address.sll_addr[6] = 0x00;
	socket_address.sll_addr[7] = 0x00;


	/*establish signal handler */
	signal(SIGINT, sigint);
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
	memcpy((void *) buffer, (void *) dest_mac, ETH_MAC_LEN);
	memcpy((void *) (buffer + ETH_MAC_LEN), (void *) src_mac,
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
				sendto(s, buffer, i + ETH_HEADER_LEN, 0,
					   (struct sockaddr *) &socket_address,
					   sizeof(socket_address));
			if (sent == -1) {
				perror("sendto():");
				exit(1);
			}

			/*Wait for incoming packet... */
			length = recvfrom(s, buffer2, BUF_SIZE, 0, NULL, NULL);
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

	free(buffer);

	printf("Client terminating....\n");

	printf("Totally sent: %ld packets\n", total_sent_packets);
	exit(0);
}
