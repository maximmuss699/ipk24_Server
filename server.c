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
volatile sig_atomic_t serverRunning = 1;

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



typedef enum {
    ACCEPT_STATE,
    AUTH_STATE,
    OPEN_STATE,
    ERROR_STATE,
    END_STATE
} State;

typedef struct {
    int fd;
    State state;
    char buffer[BUFFER_SIZE];
    char username[MAX_USERNAME_LENGTH];
    char channel[MAX_CHANNEL_ID_LENGTH];
    char secret[MAX_SECRET_LENGTH];
} Client;

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




void handle_accept_state(Client *client) {
    char displayName[MAX_DISPLAY_NAME_LENGTH];
    char auth_message[BUFFER_SIZE];
    int recv_auth = 0;

    while (recv_auth == 0) {
        int recv_len = recv(client->fd, client->buffer, BUFFER_SIZE - 1, 0);
        if (recv_len < 0) {
            perror("Error receiving data");
         //   return ERROR_STATE;
        } else if (recv_len == 0) {
            printf("Client disconnected.\n");
          //  return END_STATE;
        }

        client->buffer[recv_len] = '\0';
        printf("Received in ACCEPT_STATE: %s\n", client->buffer);
        if (strncmp(client->buffer, "AUTH ", 5) == 0) {
            if (sscanf(client->buffer, "AUTH %s %s %s", client->username, client->secret, displayName) == 3) {
                printf("Username: %s\n", client->username);
                printf("Secret: %s\n", client->secret);
                printf("Display name: %s\n", displayName);
                recv_auth = 1;
                client->state = OPEN_STATE;
            } else {
                printf("Invalid AUTH message\n");
                send(client->fd, "Invalid AUTH message\r\n", 22, 0);
            }

        }


    }
}


void handle_auth_state(Client *client) {
    while (client->state == AUTH_STATE) {
        int nbytes = read(client->fd, client->buffer, BUFFER_SIZE - 1);
        if (nbytes <= 0) {
            if (nbytes == 0) {
                printf("Client disconnected during auth state.\n");
            } else {
                perror("Read error in auth state");
            }
            client->state = END_STATE;
            break;
        }
        client->buffer[nbytes] = '\0';
        printf("Received in AUTH_STATE: %s\n", client->buffer);


       // if (authenticate(client->buffer)) {
       ///     printf("Authentication successful\n");
        //    client->state = OPEN_STATE;
     //   } else {
       //     printf("Authentication failed\n");
       //     client->state = ERROR_STATE;
       // }
    }
}

void handle_open_state(Client *client) {
    printf("Client %s connected\n", client->username);
    while (client->state == OPEN_STATE) {
        int nbytes = read(client->fd, client->buffer, BUFFER_SIZE - 1);

        client->buffer[nbytes] = '\0';
        printf("Received in OPEN_STATE: %s\n", client->buffer);


        if (strcmp(client->buffer, "BYE\r\n") == 0) {
            printf("Server received BYE\n");
            client->state = END_STATE;
        }
    }
}

void handle_error_state(Client *client) {

    send(client->fd, "Error occurred\r\n", 16, 0);
    client->state = END_STATE;
}

void handle_end_state(Client *client) {

    close(client->fd);

    client->fd = -1;

    exit (0);
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

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }



    listen(sockfd, MAX_CLIENTS);

            while (serverRunning) {
                struct sockaddr_in cli_addr; // Structure for client address
                socklen_t clilen = sizeof(cli_addr); // Size of client address
                int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
                if (newsockfd < 0) {
                    perror("ERROR on accept");
                    continue;
                }

                pid_t pid = fork();
                if (pid == 0) {
                    close(sockfd);
                    Client client;
                    client.fd = newsockfd;
                    client.state = ACCEPT_STATE;
                    memset(client.buffer, 0, BUFFER_SIZE);

                    while (client.state != END_STATE) {
                        switch (client.state) {
                            case ACCEPT_STATE:
                                handle_accept_state(&client);
                                break;
                            case AUTH_STATE:
                                handle_auth_state(&client);
                                break;
                            case OPEN_STATE:
                                handle_open_state(&client);
                                break;
                            case ERROR_STATE:
                                handle_error_state(&client);
                                break;
                            case END_STATE:
                                handle_end_state(&client);
                                break;
                            default:
                                printf("Unknown state\n");
                                client.state = END_STATE;
                        }
                    }

                    close(client.fd);
                    exit(0);
                } else if (pid > 0) {
                    close(newsockfd);
                } else {
                    perror("ERROR on fork");
                    close(newsockfd);
                }
            }
            close(sockfd);

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
