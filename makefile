CC = gcc
CFLAGS = -Wall

all: server client

server: SBCP_server2.c
	$(CC) $(CFLAGS) SBCP_server2.c -o server.out

client: SBCP_client.c
	$(CC) $(CFLAGS) SBCP_client.c -o client.out

echos: server
	./server.out

echo: client
	./client.out

clean:
	rm -f *.out
