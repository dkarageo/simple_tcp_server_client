CC=gcc

all: server_threads server_procs client

server_threads:
	$(CC) source/server_threads.c source/linked_list.c -o server_threads -O3 \
	-Wall -Wextra -lpthread

server_procs:
	$(CC) source/server_procs.c -o server_procs -O3 -Wall -Wextra

client:
	$(CC) source/client.c -o client -O3 -Wall -Wextra

clean:
	rm client server_threads server_procs
