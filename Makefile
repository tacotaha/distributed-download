CC=gcc
CFLAGS=-Wall -Werror -std=c99 -pedantic
LFLAGS=-lpthread
EXEC=server client

all: client server

server: server.o connect.o
	$(CC) server.o connect.o $(LFLAGS) -o server

client: client.o connect.o
	$(CC) client.o connect.o $(LFLAGS) -o client

server.o: Server.c
	$(CC) $(CFLAGS) -c Server.c $(LFLAGS) -o server.o

client.o: Client.c
	$(CC) $(CFLAGS) -c Client.c $(LFLAGS) -o client.o

connect.o: Connect/Connect.c
	$(CC) $(CFLAGS) -c Connect/Connect.c $(LFLAGS) -o connect.o
clean:
	rm -f $(EXEC) *.o *~
