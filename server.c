#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <search.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <pthread.h>


#define BUFFER_SIZE 1024
#define MAX_USERNAME_LENGTH 20
#define MAX_SECRET_LENGTH 128
#define MAX_DISPLAY_NAME_LENGTH 20
#define MAX_CHANNEL_ID_LENGTH 20

#define MAX_CLIENTS 100 // Maximum number of simultaneous clients
#define POLL_TIMEOUT 20000 // Timeout for poll in milliseconds

struct client {
    int fd; // client's file descriptor
    int state; // current state of the connection
    char buffer[BUFFER_SIZE]; // buffer for client data
};

// variables for handling the termination signal
bool terminateSignalReceived = false;
pthread_mutex_t terminateSignalMutex = PTHREAD_MUTEX_INITIALIZER;


// Global configuration
struct {
    char server_ip[INET_ADDRSTRLEN];
    uint16_t server_port;
    uint16_t udp_timeout;
    uint8_t udp_retries;
} config; // default values

char Display_name[MAX_DISPLAY_NAME_LENGTH + 1] = ""; // Display name for the client

typedef enum {
    ACCEPT_STATE,
    AUTH_STATE,
    OPEN_STATE,
    ERROR_STATE,
    END_STATE
} State;

void signalHandler(int signal) {
    if (signal == SIGINT) {
        pthread_mutex_lock(&terminateSignalMutex);
        terminateSignalReceived = true;
        pthread_mutex_unlock(&terminateSignalMutex);
    }
}

void print_usage() {
    printf("Usage: server [options]\n");
    printf("Options:\n");
    printf("  -l <IP address>          Server listening IP address for welcome sockets (default: 0.0.0.0)\n");
    printf("  -p <port>                Server listening port for welcome sockets (default: 4567)\n");
    printf("  -d <timeout>             UDP confirmation timeout in milliseconds (default: 250)\n");
    printf("  -r <retries>             Maximum number of UDP retransmissions (default: 3)\n");
    printf("  -h                       Prints this help output and exits\n");
}


void parse_arguments(int argc, char* argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "l:p:d:r:h")) != -1) {
        switch (opt) {
            case 'l':
                strncpy(config.server_ip, optarg, INET_ADDRSTRLEN);
                break;
            case 'p':
                config.server_port = (uint16_t)atoi(optarg);
                break;
            case 'd':
                config.udp_timeout = (uint16_t)atoi(optarg);
                break;
            case 'r':
                config.udp_retries = (uint8_t)atoi(optarg);
                break;
            case 'h':
                print_usage();
                exit(EXIT_SUCCESS);
            default:
                print_usage();
                exit(EXIT_FAILURE);
        }
    }

}



int accept_state(int sockfd) {
    struct pollfd fds[MAX_CLIENTS]; // Array of pollfd structures for poll()
    struct client clients[MAX_CLIENTS]; // Client state information
    int nfds = 1; // Number of file descriptors being polled

// Initialize all client fds to -1 for poll
    for (int i = 0; i < MAX_CLIENTS; i++) {
        fds[i].fd = -1;
        fds[i].events = POLLIN;
        clients[i].fd = -1; // Indicate unused slot
    }

    listen(sockfd, MAX_CLIENTS);
    fds[0].fd = sockfd; // Listening socket
    fds[0].events = POLLIN;
    clients[0].fd = sockfd; // Set listening socket in clients array
    clients[0].state = ACCEPT_STATE;

    while (1) {

        pthread_mutex_lock(&terminateSignalMutex);
        if (errno == EINTR && terminateSignalReceived) {
            pthread_mutex_unlock(&terminateSignalMutex);
            exit(0);
        }
        pthread_mutex_unlock(&terminateSignalMutex);

        int poll_count = poll(fds, nfds, POLL_TIMEOUT);
        if (poll_count < 0) {
            perror("Poll failed");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (fds[i].revents & POLLIN) // Check for incoming data
            {
                if (fds[i].fd == sockfd) {
                    // Handle new connection
                    struct sockaddr_in cli_addr;
                    socklen_t clilen = sizeof(cli_addr);
                    int newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
                    if (newsockfd < 0) {
                        perror("ERROR on accept");
                        continue;
                    }
                    // Add new client
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].fd < 0) {
                            clients[j].fd = newsockfd;
                            clients[j].state = AUTH_STATE; // Initial state
                            fds[j].fd = newsockfd;
                            fds[j].events = POLLIN;
                            if (j >= nfds) nfds = j + 1;
                            break;
                        }
                    }
                } else {
                    // Handle existing client
                    int n = read(fds[i].fd, clients[i].buffer, BUFFER_SIZE - 1);
                    if (n <= 0) {
                        // Client closed connection or error
                        if (n < 0) {
                            perror("ERROR reading from socket");
                        } else {
                            printf("Client disconnected\n");
                        }
                        close(fds[i].fd);
                        fds[i].fd = -1; // Remove from poll set
                        clients[i].fd = -1;
                    } else {
                        // Ensure the string is null-terminated
                        clients[i].buffer[n] = '\0';

                        // Process client request based on current state
                        switch (clients[i].state) {
                            case AUTH_STATE:
                                // Assume AUTH_STATE completes here and transitions to OPEN_STATE
                                // For demonstration, let's just print and transition
                                printf("Authentication request: %s\n", clients[i].buffer);
                                clients[i].state = OPEN_STATE;  // Transition to OPEN_STATE after authentication
                                break;
                            case OPEN_STATE:
                                // Log or print the message received from the client
                                printf("Message from client: %s\n", clients[i].buffer);
                                // Here you could add additional logic to handle the message further
                                break;
                            case ERROR_STATE:
                            case END_STATE:
                                // Clean up and close
                                close(fds[i].fd);
                                fds[i].fd = -1; // Remove from poll set
                                clients[i].fd = -1;
                                break;
                        }
                    }
                }
            }
        }
    }



}



void FSM_function() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(config.server_ip);
    serv_addr.sin_port = htons(config.server_port);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }
    State state = ACCEPT_STATE;
    while (1) {
        switch (state) {
            case ACCEPT_STATE:

                state = accept_state(sockfd);
                break;
            case AUTH_STATE:

                // state = AUTH_STATETCP(sock, state);
                break;
            case OPEN_STATE:

                //state = Open_stateTCP(sock, state);
                break;
            case ERROR_STATE:

                // state = Error_stateTCP(sock);
                break;
            case END_STATE:

                // state = End_stateTCP(sock);
                break;
            default:
                perror("Invalid state");
                exit(EXIT_FAILURE);
        }

    }
}

int main(int argc, char *argv[]){
    if (argc == 1) {
        fprintf(stderr, "ERR: No arguments given.\n");
        print_usage();
        exit(1);
    }
    //
    parse_arguments(argc, argv);
    signal(SIGINT, signalHandler);

    //
    if (config.server_ip == NULL || strcmp(config.server_ip, "") == 0) {
        fprintf(stderr, "Error: bad IP\n");
        exit(EXIT_FAILURE);
    }

    if(config.udp_retries < 0 || config.udp_timeout < 0){
        fprintf(stderr, "Error: bad number of retransmission or timeout\n");
        exit(EXIT_FAILURE);
    }


    FSM_function();

    return 0;


}
