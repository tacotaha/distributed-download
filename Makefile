CC=gcc
CFLAGS=-Wall -Werror -std=c99 -pedantic
LFLAGS=-lpthread -lssl -lcrypto
EXEC=server client

all: client server

server: server.o common.o
	$(CC) server.o common.o $(LFLAGS) -o server

client: client.o common.o
	$(CC) client.o common.o $(LFLAGS) -o client

server.o: Server.c
	$(CC) $(CFLAGS) -c Server.c $(LFLAGS) -o server.o

client.o: Client.c
	$(CC) $(CFLAGS) -c Client.c $(LFLAGS) -o client.o

common.o: Common/Common.c
	$(CC) $(CFLAGS) -c Common/Common.c $(LFLAGS) -o common.o
clean:
	rm -f $(EXEC) *.o *~
