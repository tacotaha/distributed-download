CC=gcc
CFLAGS=-Wall -Werror -std=c99 -pedantic
PFLAGS=-lpthread
EXEC=server client

all: client server

server: server.o connect.o
	$(CC) server.o connect.o $(PFLAGS) -o server

client: client.o connect.o
	$(CC) client.o connect.o $(PFLAGS) -o client

server.o: Server.c
	$(CC) $(CFLAGS) -c Server.c -o server.o

client.o: Client.c
	$(CC) $(CFLAGS) -c Client.c -o client.o

connect.o: Connect/Connect.c
	$(CC) $(CFLAGS) -c Connect/Connect.c -o connect.o
clean:
	rm -f $(EXEC) *.o *~
