/**
 * server_threads.c
 *
 * Created by Dimitrios Karageorgiou,
 *  for course "Embedded And Realtime Systems".
 *  Electrical and Computers Engineering Department, AuTh, GR - 2017-2018
 *
 * A TCP server able to handle multiple connections in a thread based model.
 *
 * Usage: exec_name <port>
 *  where:
 *      -port : Port number on which to start the server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "linked_list.h"


typedef struct {
    int socket_fd;
    struct sockaddr_in addr;
} handler_args_t;

typedef struct {
    pthread_t tid;
    int fd;
} handler_t;


int init_listener(int port);
void start_listener(int socket_fd);
void destroy_listener(int socket_fd);
void handle_client(int client_fd, struct sockaddr_in client_addr);
void *start_handler(void *args);
void error(const char *msg);
void terminate_server(int signum);


linked_list_t *handler_fds;  // Storage for all socket descriptors of active
                             // handlers.
int listener_fd;             // Socket descriptor of listener.
pthread_mutex_t *list_mutex; // A mutex used for list operations.


int main(int argc, char *argv[])
{
    // Listening port should be provided by caller.
    if (argc < 2) {
        fprintf(stderr, "ERROR: No listening port provided.\n");
        fprintf(stdout, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    handler_fds = linked_list_create();
    list_mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(list_mutex, NULL);

    int port = atoi(argv[1]); // Listening port.
    listener_fd = init_listener(port);

    signal(SIGINT, terminate_server);
    printf("Use CTRL+C to terminate.\n");

    // Use current thread for the listener.
    start_listener(listener_fd);

    pthread_exit(0);  // Make sure that process terminates only when handlers
                      // have terminated too
}

/**
 * Prints a message about currently set errno and terminates process.
 */
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

/**
 * Ask server to terminate normally completing any critical unhandled task.
 *
 * This is a signal handler, that should be connected to a terminating signal.
 */
void terminate_server(int signum)
{
    printf("\nServer terminating...\n");

    // Stop accepting new connections.
    destroy_listener(listener_fd);

    linked_list_t *tids = linked_list_create();  // Store handlers to wait for.

    iterator_t *iter;

    // Ask active handlers to terminate and extract their IDs.
    pthread_mutex_lock(list_mutex);
    iter = linked_list_iterator(handler_fds);
    while(iterator_has_next(iter)) {
        handler_t *handler = (handler_t *) iterator_next(iter);

        // Shutdown the socket connection.
        int rc = shutdown(handler->fd, SHUT_RDWR);
        if (rc == -1) error("Connection shutdown error");

        // Keep id for waiting later.
        linked_list_append(tids, (void *) handler->tid);
    }
    iterator_destroy(iter);
    pthread_mutex_unlock(list_mutex);

    // Wait for previous active handlers to terminate (maybe already done so).
    iter = linked_list_iterator(tids);
    while(iterator_has_next(iter)) {
        pthread_t tid = (pthread_t) iterator_next(iter);
        pthread_join(tid, NULL);
    }
    iterator_destroy(iter);
    linked_list_destroy(tids);

    // Now globals can be destroyed safely.
    pthread_mutex_destroy(list_mutex);
    free(list_mutex);
    linked_list_destroy(handler_fds);
}

/**
 * Initialize a listener on the given port.
 */
int init_listener(int port)
{
    int socket_fd;                 // Listener's file descriptor.
    struct sockaddr_in serv_addr;  // Server's local address.

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);  // IPv4 TCP socket.
    if (socket_fd < 0) error("ERROR: Opening of socket failed");

    // Create a sockaddr object with local IP and listening port.
    memset((void *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    // Bind to the listening port at localhost.
    if (bind(socket_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR: Binding failed");
    }

    return socket_fd;
}

/**
 * Converts current thread into a listener on given socket.
 */
void start_listener(int socket_fd)
{
    int rc = listen(socket_fd, 8);  // Mark socket as listener.
    if (rc < 0) error("ERROR: Failed to listen on given socket");

    int in_fd;  // File descriptor for incoming connection.
    struct sockaddr_in client_addr;  // Address object of the client.
    socklen_t sock_size = sizeof(client_addr);

    while ((in_fd = accept(socket_fd,
                           (struct sockaddr *) &client_addr,
                           &sock_size)) > -1)
    {
        handle_client(in_fd, client_addr);
    }
}

/**
 * Properly terminates a listener on the given socket.
 */
void destroy_listener(int socket_fd)
{
    close(socket_fd);
}

/**
 * Create a handler on a new thread for the given client connection.
 */
void handle_client(int client_fd, struct sockaddr_in client_addr)
{
    handler_args_t *args = (handler_args_t *) malloc(sizeof(handler_args_t));
    args->socket_fd = client_fd;
    args->addr = client_addr;

    pthread_t tid;
    pthread_create(&tid, NULL, start_handler, (void *) args);
}

/**
 * Entry point for new handler's thread.
 */
void *start_handler(void *args)
{
    handler_args_t *h_args = (handler_args_t *) args;
    // printf("New connection accepted. FD: %d\n", h_args->socket_fd);

    // Add self to the list of active handlers.
    handler_t *handler = malloc(sizeof(handler_t));
    handler->tid = pthread_self();
    handler->fd = h_args->socket_fd;

    pthread_mutex_lock(list_mutex);
    node_t *node = linked_list_append(handler_fds, (void *) handler);
    pthread_mutex_unlock(list_mutex);

    // Initialize incoming message buffer.
    char *buffer = (char *) malloc(sizeof(char) * 256);
    memset(buffer, 0, 256);
    int n;

    // Keep reading till an error or shutdown (n == 0).
    while((n = read(h_args->socket_fd, buffer, 255)) > 0) {
        printf("%s", buffer);
        memset(buffer, 0, 256);  // Clear the buffer for next incoming.
    }

    // Remove handler from list before terminating.
    pthread_mutex_lock(list_mutex);
    handler = linked_list_remove(handler_fds, node);
    free(handler);  // Also keep in mutex, to properly handle server termination.
    pthread_mutex_unlock(list_mutex);

    // Finally, close the connection to the client.
    close(h_args->socket_fd);

    // Free local resources.
    free(buffer);
    free(args);

    // printf("Connection closed.\n");
    pthread_exit(0);
}
