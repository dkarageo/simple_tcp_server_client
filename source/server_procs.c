/**
 * server_procs.c
 *
 * Created by Dimitrios Karageorgiou,
 *  for course "Embedded And Realtime Systems".
 *  Electrical and Computers Engineering Department, AuTh, GR - 2017-2018
 *
 * A TCP server able to handle multiple connections in a process based model.
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
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


int init_listener(int port);
void start_listener(int socket_fd);
void destroy_listener(int socket_fd);
void handle_client(int client_fd, struct sockaddr_in client_addr);
void error(const char *msg);
void terminate_server(int signum);
void terminate_handler(int signum);


int listener_fd; // Handler of the listener connection (valid to listener).
int handler_fd;  // Handler of the connection to client (valid to handlers).


int main(int argc, char *argv[])
{
    // Listening port should be provided by caller.
    if (argc < 2) {
        fprintf(stderr, "ERROR: No listening port provided.\n");
        fprintf(stdout, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]); // Listening port.
    listener_fd = init_listener(port);

    signal(SIGINT, terminate_server);
    printf("Use CTRL+C to terminate.\n");

    // Use current process for the listener.
    start_listener(listener_fd);

    return 0;
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
 * This is a signal handler, that should be connected to a terminating signal
 * in the listener process.
 */
void terminate_server(int signum)
{
    printf("\nServer terminating...\n");

    // Stop accepting new connections.
    destroy_listener(listener_fd);

    // Wait for all handler processes to complete.
    int pid;
    while((pid = wait(NULL)) > 0);
}

/**
 * Ask a handler process to terminate.
 *
 * This is a signal handler that should be connected to the same terminating
 * signal terminate_server handler was connected to, on all handler processes.
 */
void terminate_handler(int signum)
{
    shutdown(handler_fd, SHUT_RDWR);
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
 * Converts current process into a listener on given socket.
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
        int pid;
        if ((pid = fork()) == 0) {
            // Handle the new client by a new process.
            handle_client(in_fd, client_addr);
        }
        else if (pid == -1)  {
            error("ERROR: Failed to launch handler");
        }
        else {
            close(in_fd);  // On listener (parent) close this socket.
        }
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
 * Start the handler.
 */
void handle_client(int client_fd, struct sockaddr_in client_addr)
{
    // printf("New connection accepted. FD: %d\n", h_args->socket_fd);
    signal(SIGINT, terminate_handler);
    handler_fd = client_fd;

    // Initialize incoming message buffer.
    char *buffer = (char *) malloc(sizeof(char) * 256);
    memset(buffer, 0, 256);
    int n;

    // Keep reading till an error or shutdown (n == 0).
    while((n = read(client_fd, buffer, 255)) > 0) {
        printf("%s", buffer);
        memset(buffer, 0, 256);  // Clear the buffer for next incoming.
    }

    // Close the connection to the client.
    close(client_fd);

    // Free local resources.
    free(buffer);

    // printf("Connection closed.\n");
    exit(0);
}
