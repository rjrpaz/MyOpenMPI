/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2006 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2007-2008 Sun Microsystems, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdlib.h>

#include "orte/util/show_help.h"
#include "ompi/mpi/c/bindings.h"
#include "ompi/constants.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_Myinit = PMPI_Myinit
#endif

#if OMPI_PROFILING_DEFINES
#include "ompi/mpi/c/profile/defines.h"
#endif


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
/*
#include <netinet/in.h>
#include <net/if.h>
*/

#include <linux/if_ether.h>
#include <linux/if_arp.h>

#undef DEBUG

#define DEVICE "eth0"

int my_size, my_rank;
my_sr_node *my_nodes;
MPI_Datatype Rankaddr;
unsigned char my_mac_address[6];
int my_socket_raw_send, my_socket_raw_recv, my_ifindex;
struct sockaddr_ll my_socket_address;


static const char FUNC_NAME[] = "MPI_Myinit";


int MPI_Myinit(int *argc, char ***argv)
{
	int err;
	int provided;
	char *env;
	int required = MPI_THREAD_SINGLE;

	int my_blocklen[2] = {2, 6};
	MPI_Aint my_disp[2];
	MPI_Aint start_address, address;
	MPI_Datatype my_type[2] = {MPI_INT, MPI_CHAR};
	int i;
	struct ifreq my_ifr;
	MPI_Status my_status;

#ifdef DEBUG
  printf("Ingresando a MPI_Myinit()\n");
#endif

	if (NULL != argc && NULL != argv)
		err = MPI_Init(argc, argv);
	else
		err = MPI_Init(0, NULL);

	if (MPI_SUCCESS != err) {
		return ompi_errhandler_invoke(NULL, NULL, OMPI_ERRHANDLER_TYPE_COMM, 
                                    err < 0 ? ompi_errcode_get_mpi_code(err) : 
                                    err, FUNC_NAME);
	}

	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &my_size);

	if (my_size > 1) {

		/* Determino mi propia mac address para socket raw */
		my_socket_raw_send = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
		if (my_socket_raw_send == -1) {
			perror("socket() para send:");
			return(MPI_ERR_SOCKET_RAW);
		}

		my_socket_raw_recv = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
		if (my_socket_raw_recv == -1) {
			perror("socket() para send:");
			return(MPI_ERR_SOCKET_RAW);
		}


#ifdef DEBUG
//  printf("Successfully opened socket: %i\n", sd);
    printf("Successfully opened my_socket_raw_send: %i\n", my_socket_raw_send);
    printf("Successfully opened my_socket_raw_recv: %i\n", my_socket_raw_recv);
#endif


		strncpy(my_ifr.ifr_name, DEVICE, IFNAMSIZ);
		if (ioctl(my_socket_raw_send, SIOCGIFHWADDR, &my_ifr) == -1) {
			perror("SIOCGIFINDEX");
			return(MPI_ERR_SOCKET_RAW);
		}
		my_ifindex = my_ifr.ifr_ifindex;
//		close (my_socket_raw);

		my_socket_address.sll_family = PF_PACKET;
		my_socket_address.sll_protocol = htons(ETH_P_IP);
		my_socket_address.sll_ifindex = my_ifindex;
		my_socket_address.sll_hatype = ARPHRD_ETHER;
		my_socket_address.sll_pkttype = PACKET_OTHERHOST;
		my_socket_address.sll_halen = ETH_ALEN;
		my_socket_address.sll_addr[6] = 0x00;
		my_socket_address.sll_addr[7] = 0x00;

		if (my_rank == 0) {
			my_nodes = malloc(my_size * sizeof(struct my_list_node));
		} else {
			my_nodes = malloc(sizeof(struct my_list_node));
		}

		my_disp[0] = 0;
		MPI_Get_address(&(my_nodes->rank), &start_address);
		MPI_Get_address(&(my_nodes->addr), &address);
		my_disp[1] = address - start_address;

		MPI_Type_create_struct(2, my_blocklen, my_disp, my_type, &Rankaddr);
		MPI_Type_commit(&Rankaddr);

        my_nodes->rank[0] = my_rank;
        my_nodes->rank[1] = my_rank*my_rank;
		for (i = 0; i < 6; i++)
        	my_nodes->addr[i] = (unsigned char)my_ifr.ifr_hwaddr.sa_data[i];
		memcpy((void *) &my_mac_address[0], (const void *) &my_nodes->addr[0], 6);


//		printf("MAC ADDRESS ADENTRO es: %02X:%02X:%02X:%02X:%02X:%02X\n", my_nodes->addr[0],my_nodes->addr[1],my_nodes->addr[2],my_nodes->addr[3],my_nodes->addr[4],my_nodes->addr[5]);

//      printf("ANTES %d %d\n", my_nodes->rank[0], my_nodes->rank[1]);
		if (my_rank == 0) {
//      printf("TAMANO %d\n", sizeof(struct my_node));
			for (i=1;i<my_size;i++) {
				MPI_Recv(my_nodes+i, 1, Rankaddr, i, i, MPI_COMM_WORLD, &my_status);
#ifdef DEBUG
              printf("RECIBIDO %d desde RANK: %d\n", (my_nodes+i)->rank[0], i);
#endif
			}
		} else {
			MPI_Send(my_nodes, 1, Rankaddr, 0, my_rank, MPI_COMM_WORLD);
#ifdef DEBUG
          printf("ENVIANDO %d desde RANK %d Almacenado en direccion: %p\n", my_nodes->rank[1], my_rank, &(my_nodes->rank[1]));
#endif
		}
	}

#ifdef DEBUG
	if (my_rank == 0) {
		for (i=0;i<my_size;i++) {
			printf("MAC Address de RANK %d es: %02X:%02X:%02X:%02X:%02X:%02X\n", i, (my_nodes+i)->addr[0],(my_nodes+i)->addr[1],(my_nodes+i)->addr[2],(my_nodes+i)->addr[3],(my_nodes+i)->addr[4],(my_nodes+i)->addr[5]);
		}
	}
#endif

  return MPI_SUCCESS;

#if 0
  /* Ensure that we were not already initialized or finalized */
  if (ompi_mpi_finalized) {
      if (0 == ompi_comm_rank(MPI_COMM_WORLD)) {
          orte_show_help("help-mpi-api.txt", "mpi-function-after-finalize",
                         true, FUNC_NAME);
      }
      return ompi_errhandler_invoke(NULL, NULL, OMPI_ERRHANDLER_TYPE_COMM, 
                                    MPI_ERR_OTHER, FUNC_NAME);
  } else if (ompi_mpi_initialized) {
      if (0 == ompi_comm_rank(MPI_COMM_WORLD)) {
          orte_show_help("help-mpi-api.txt", "mpi-initialize-twice",
                         true, FUNC_NAME);
      }
      return OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, MPI_ERR_OTHER, FUNC_NAME);
  }

  /* check for environment overrides for required thread level.  If
     there is, check to see that it is a valid/supported thread level.
     If not, default to MPI_THREAD_MULTIPLE. */

  if (NULL != (env = getenv("OMPI_MPI_THREAD_LEVEL"))) {
    required = atoi(env);
    if (required < MPI_THREAD_SINGLE || required > MPI_THREAD_MULTIPLE) {
      required = MPI_THREAD_MULTIPLE;
    }
  } 

  /* Call the back-end initialization function (we need to put as
     little in this function as possible so that if it's profiled, we
     don't lose anything) */

  if (NULL != argc && NULL != argv) {
      err = ompi_mpi_init(*argc, *argv, required, &provided);
  } else {
      err = ompi_mpi_init(0, NULL, required, &provided);
  }

  /* Since we don't have a communicator to invoke an errorhandler on
     here, don't use the fancy-schmancy ERRHANDLER macros; they're
     really designed for real communicator objects.  Just use the
     back-end function directly. */

  if (MPI_SUCCESS != err) {
      return ompi_errhandler_invoke(NULL, NULL, OMPI_ERRHANDLER_TYPE_COMM, 
                                    err < 0 ? ompi_errcode_get_mpi_code(err) : 
                                    err, FUNC_NAME);
  }

  OPAL_CR_INIT_LIBRARY();

	MPI_Barrier(MPI_COMM_WORLD);
  return MPI_SUCCESS;
#endif
}
