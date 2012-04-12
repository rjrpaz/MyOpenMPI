/*
 * Copyright (c) 2007      Sun Microsystems, Inc.  All rights reserved.
 */

/*
 * Test the connectivity between all processes.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <mpi.h>

#define TIMES 1
#define SIZE 1

int main(int argc, char **argv)
{
	MPI_Status status;
	int verbose = 0;
	int rank;
	int np;						/* number of processes in job */
	int i;
	int j;
	int k;
	int *enviado, *recibido;
	int length;
	char name[MPI_MAX_PROCESSOR_NAME + 1];

//	MPI_Init(&argc, &argv);
	MPI_Myinit(&argc, &argv);

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &np);

	/*
	 * If we cannot get the name for whatever reason, just
	 * set it to unknown. */
	if (MPI_SUCCESS != MPI_Get_processor_name(name, &length)) {
		strcpy(name, "unknown");
	}

	if (argc > 1 && strcmp(argv[1], "-v") == 0)
		verbose = 1;


	enviado = malloc(SIZE * sizeof(int));
	recibido = malloc(SIZE * sizeof(int));
	for (i = 0; i < SIZE; i++) {
		*(enviado + i) = rank + i;
	}

	for (k = 0; k < TIMES; k++) {
		printf("--PASO NRO %d en rank %d\n", k, rank);
		for (i = 0; i < np; i++) {
			if (rank == i) {
				/* rank i sends to and receives from each higher rank */
				for (j = i + 1; j < np; j++) {
					if (verbose)
						printf
							("--checking connection between rank %d on %s and rank %-4d\n",
							 i, name, j);
//					MPI_Send(enviado, SIZE, MPI_INT, j, rank, MPI_COMM_WORLD);
					MPI_Mysend(enviado, SIZE, MPI_INT, j, rank, MPI_COMM_WORLD);
					printf("--EL VALOR RECIBIDO ANTES %d en dirección %p\n",
						   *recibido, recibido);
//					MPI_Recv(recibido, SIZE, MPI_INT, j, j, MPI_COMM_WORLD, &status);
					MPI_Myrecv(recibido, SIZE, MPI_INT, j, j, MPI_COMM_WORLD, &status);
					printf
						("--EN RANK %d, EL VALOR RECIBIDO ES %d en dirección %p\n",
						 rank, *recibido, &recibido);
				}
			} else if (rank > i) {
				/* receive from and reply to rank i */
				printf("--EL VALOR RECIBIDO ANTES %d en dirección %p\n",
					   *recibido, recibido);
//				MPI_Recv(recibido, SIZE, MPI_INT, i, i, MPI_COMM_WORLD, &status);
				MPI_Myrecv(recibido, SIZE, MPI_INT, i, i, MPI_COMM_WORLD, &status);
				printf
					("--EN RANK %d, EL VALOR RECIBIDO ES %d en dirección %p\n",
					 rank, *recibido, recibido);
//				MPI_Send(enviado, SIZE, MPI_INT, i, rank, MPI_COMM_WORLD);
				MPI_Mysend(enviado, SIZE, MPI_INT, i, rank, MPI_COMM_WORLD);
			}
		}
	}

	MPI_Barrier(MPI_COMM_WORLD);


	if (rank == 1) {
		printf("--Connectivity test on %d processes PASSED.\n", np);
	}

//	MPI_Finalize();
	MPI_Myfinalize();

//	free(recibido);
//	free(enviado);
	return 0;
}
