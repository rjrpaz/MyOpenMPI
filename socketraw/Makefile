CFLAGS=-Wall -O2
#CFLAGS=-Wall -O2 -DDEBUG

all: client server

client: client.c common.c
	gcc ${CFLAGS} -o client client.c common.c

server: server.c common.c
	gcc ${CFLAGS} -o server server.c common.c

indent:
	indent -ts 4 -kr *.c

clean:
	rm -f client server

