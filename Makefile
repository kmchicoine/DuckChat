CC=g++

CFLAGS=-Wall -W -g


all: client server

client: client.c raw.c
	$(CC) client.c raw.c $(CFLAGS) -o client

server: server.c
	$(CC) server.c $(CFLAGS) -o server -lrt

clean:
	rm -f client server *.o

