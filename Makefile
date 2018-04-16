CC=gcc

all: server_threads server_procs client

server_threads:
	$(CC) source/server_threads.c source/linked_list.c -o server_threads -O3 \
	-Wall -Wextra -lpthread -g

server_procs:
	$(CC) source/server_procs.c source/linked_list.c -o server_procs -O3 -Wall -Wextra -g

client:
	$(CC) source/client.c -o client -O3 -Wall -Wextra -g

clean:
	rm client server_threads server_procs
