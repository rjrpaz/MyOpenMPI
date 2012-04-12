#include "ethertest.h"

void print_eth_mac_addr(unsigned char* eth_header); /*prints both MAC adresses from Ethernet Header*/
void hexdump(unsigned char *buf, int nbytes);	    /*hexdumps some memory to stdout*/	
int create_udp_socket(int port);		    /*returns a socket despriptor the a newly created UDP Socket*/	

#define MTU 1500
#define IP_H_SIZE 20
#define TCP_H_SIZE 32
#define MPICH_H_SIZE 22
#define DATASIZE (MTU - IP_H_SIZE - TCP_H_SIZE - MPICH_H_SIZE)

