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
#include "linked_list.h"


typedef struct {
    int fd;
} handler_t;


int init_listener(int port);
void start_listener(int socket_fd);
void destroy_listener(int socket_fd);
void handle_client(int client_fd, struct sockaddr_in client_addr, node_t *node);
void error(const char *msg);
void terminate_server(int signum);
void remove_handler(int signum, siginfo_t *info, void *cont);


const int TERM_SIGNAL = SIGINT;  // Signal for requesting server termination.
const int MAN_SIGNAL = SIGUSR1;  // Signal used for manipulating handler_fds.

struct sigaction man_act;
struct sigaction act;

// Globals valid to listener process only.
linked_list_t *handler_fds;   // Storage for info of active handlers.
int listener_fd; // Handler of the listener connection.
sigset_t *blocked_signals;  // Signals to block when manipulating handler_fds.

// Globals valid to handlers processes only.
int handler_fd;  // Handler of the connection to client.


int main(int argc, char *argv[])
{
    // Listening port should be provided by caller.
    if (argc < 2) {
        fprintf(stderr, "ERROR: No listening port provided.\n");
        fprintf(stdout, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // Initialize globals.
    handler_fds = linked_list_create();
    blocked_signals = (sigset_t *) malloc(sizeof(sigset_t));
    sigemptyset(blocked_signals);
    sigaddset(blocked_signals, MAN_SIGNAL);

    int port = atoi(argv[1]); // Listening port.
    listener_fd = init_listener(port);

    // Install signal handler for list manipilation signal.
    memset(&man_act, 0, sizeof(struct sigaction));
    man_act.sa_sigaction = remove_handler;
    man_act.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(MAN_SIGNAL, &man_act, NULL);

    // Install signal handler for terminating signal.
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = terminate_server;
    sigaction(TERM_SIGNAL, &act, NULL);
    printf("Use CTRL+C to terminate.\n");

    // Use current process for the listener.
    start_listener(listener_fd);

    printf("\nServer terminating...\n");

    sigprocmask(SIG_BLOCK, blocked_signals, NULL);
    iterator_t * iter = linked_list_iterator(handler_fds);
    while(iterator_has_next(iter)) {
        handler_t *handler = (handler_t *) iterator_next(iter);
        shutdown(handler->fd, SHUT_RDWR);
    }
    iterator_destroy(iter);
    sigprocmask(SIG_UNBLOCK, blocked_signals, NULL);

    // Wait for all handler processes to complete.
    int pid;
    while((pid = wait(NULL)) > 0);

    // Cleanup resources.
    free(blocked_signals);
    linked_list_destroy(handler_fds);

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
    // Stop accepting new connections.
    if (signum == TERM_SIGNAL) destroy_listener(listener_fd);
}

/**
 * Remove a handler from the list of active handlers.
 *
 * This is a signal handler for a signal that should be sent from each handler
 * process just before it terminatess.
 */
void remove_handler(int signum, siginfo_t *info, void *cont)
{
    if (signum == MAN_SIGNAL) {
        printf("Removing handler...\n");
        handler_t * handler = linked_list_remove(
                handler_fds, info->si_value.sival_ptr);
        close(handler->fd);  // Listener no more needs an open fd to client.
        free(handler);
    }
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
        // Add a new entry to handlers list.
        handler_t *handler = malloc(sizeof(handler_t));
        handler->fd = in_fd;
        sigprocmask(SIG_BLOCK, blocked_signals, NULL);
    	node_t *node = linked_list_append(handler_fds, (void *) handler);
        sigprocmask(SIG_UNBLOCK, blocked_signals, NULL);

        int pid;
        if ((pid = fork()) == 0) {
            close(socket_fd);
            // Handle the new client by a new process.
            handle_client(in_fd, client_addr, node);
        }
        else if (pid == -1)  {
            error("ERROR: Failed to launch handler");
        }
        // Else: Do not close in_fd in parent process, Otherwise it will be
        // impossible to shutdown() the connection.
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
void handle_client(int client_fd, struct sockaddr_in client_addr, node_t *node)
{
    // printf("New connection accepted. FD: %d\n", h_args->socket_fd);

    // Disable termination signal.
    sigset_t sigs;
    sigemptyset(&sigs);
    sigaddset(&sigs, TERM_SIGNAL);
    sigprocmask(SIG_BLOCK, &sigs, NULL);

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

    // Signal parent process to treat this handler as dead.
    union sigval val;
    val.sival_ptr = (void *) node;
    sigqueue(getppid(), MAN_SIGNAL, val);
    // kill(getppid(), MAN_SIGNAL);

    // Close the connection to the client.
    close(client_fd);

    // Free local resources.
    free(buffer);

    printf("Connection closed.\n");
    exit(0);
}
