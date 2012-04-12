#CFLAGS=-Wall -O2
CFLAGS=-Wall -O2 -DDEBUG

all: client server

client: client.c
	gcc ${CFLAGS} -o client client.c

server: server.c
	gcc ${CFLAGS} -o server server.c

clean:
	rm -f client server

