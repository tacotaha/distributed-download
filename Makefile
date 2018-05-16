CC=gcc
CFLAGS=-Wall -Werror -std=c99 -pedantic
PFLAGS=-lpthread
EXEC=server client

all: client server

connect.o: Connect.c Connect.h
	$(CC) $(CFLAGS) -c Connect.c -o connect.o
clean:
	rm -f $(EXEC) *.o *~
