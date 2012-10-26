extern void error(char *);
extern void warning(char *);
extern void dump_buffer(unsigned char *, int);

/*
 *
 * Definitions that are used in server and client
 *
 */

// #include <linux/if_ether.h> 

/* see linux/if_ether.h */
#define ETH_MAC_LEN        ETH_ALEN      /* Octets in one ethernet addr   */
#define ETH_HEADER_LEN     ETH_HLEN      /* Total octets in header.       */
#define ETH_MIN_LEN        60            /* Tamaño mínimo de un paquete ethernet */
#define RAW_HEADER_LEN     2      		 /* Total octets in custom header.*/
#define ETH_MIN_FRAME_LEN  ETH_ZLEN      /* Min. octets in frame sans FCS */
#define ETH_USER_DATA_LEN  ETH_DATA_LEN  /* Max. octets in payload        */
#define ETH_MAX_FRAME_LEN  ETH_FRAME_LEN /* Max. octets in frame sans FCS */

#define ETH_FRAME_TOTALLEN 1518          /*Header: 14 + User Data: 1500 FCS: 4*/

// Consulfem
#define DEVICE             "eth0"	/* Device used for communication*/
// Casa
// #define DEVICE             "wlan0"

#define ETH_P_NULL	   0x0		/* we are running without any protocol above the Ethernet Layer*/


//#define BUFFER_SIZE ETH_FRAME_TOTALLEN
#define BUFFER_SIZE 1517
//#define CHUNK 1024
// 1500 bytes - 2 cabecera propietaria
//#define CHUNK ETH_FRAME_TOTALLEN-ETH_HEADER_LEN-RAW_HEADER_LEN
#define CHUNK 1498

#define CANT_CHUNKS 16
