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
#define MAX_CHANNELS 100 // Maximum number of channels

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
} config;



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


typedef struct {
    char channelName[MAX_CHANNEL_ID_LENGTH];
    Client *clients[MAX_CLIENTS];  // Pointers to clients in this channel
    int clientCount;
} Channel;

Channel channels[MAX_CHANNELS]; // Array to hold all channels
Client clients[MAX_CLIENTS];  // Global client list

bool allowMultipleConnections = false;  // Toggleable feature


Channel* get_or_create_channel(const char* channelName) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (strcmp(channels[i].channelName, channelName) == 0) {
            return &channels[i]; // Channel exists
        }
    }

    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].channelName[0] == '\0') { // Empty slot
            strcpy(channels[i].channelName, channelName);
            channels[i].clientCount = 0;
            return &channels[i]; // Newly created channel
        }
    }

    return NULL; // No available slot or max channels reached
}

void leave_channel(Client *client) {
    if (client->channel[0] == '\0')
        return; // Client is not in any channel

    Channel* channel = get_or_create_channel(client->channel);
    if (channel) {
        for (int i = 0; i < channel->clientCount; i++) {
            if (channel->clients[i] == client) {
                // Shift the rest of the clients down in the array
                memmove(&channel->clients[i], &channel->clients[i + 1], (channel->clientCount - i - 1) * sizeof(Client*));
                channel->clientCount--;
                break;
            }
        }
    }

    client->channel[0] = '\0'; // Remove channel from client
}


int join_channel(Client *client, const char* channelName) {
    // Leave any previous channel
    leave_channel(client);

    Channel* channel = get_or_create_channel(channelName);
    if (!channel) {
        return -1; // Failed to create or find channel
    }

    if (channel->clientCount < MAX_CLIENTS) {
        channel->clients[channel->clientCount++] = client;
        strcpy(client->channel, channelName); // Update client's current channel
        return 0;
    }

    return -1; // Channel is full
}

Channel* find_channel_by_id(const char* channelID) {
    // Assuming there is a global array or list of channels
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (strcmp(channels[i].channelName, channelID) == 0) {
            return &channels[i];
        }
    }
    return NULL;
}

void broadcast_message(Channel *channel, const char *message, Client *sender) {
    Channel *CHANNEL = find_channel_by_id(channel->channelName);
    if (!CHANNEL) {
        fprintf(stderr, "Channel %s not found.\n", channel->channelName);
        return;
    }
    for (int i = 0; i < CHANNEL->clientCount; i++) {
        if (CHANNEL->clients[i] != sender) { // Ensure not to send the message to the sender
            send(CHANNEL->clients[i]->fd, message, strlen(message), 0);
            printf("Sent to %s: %s\n", CHANNEL->clients[i]->username, message); // Debug output
        }
    }


}



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

void log_message(const char* prefix, int fd, const char* message) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getpeername(fd, (struct sockaddr *)&addr, &addr_len) == -1) {
        perror("getpeername failed");
        return;
    }

    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, clientIP, INET_ADDRSTRLEN);
    int clientPort = ntohs(addr.sin_port);

    printf("%s %s:%d | %s\n", prefix, clientIP, clientPort, message);
}


void handle_accept_state(Client *client) {
    char displayName[MAX_DISPLAY_NAME_LENGTH];
    char auth_message[BUFFER_SIZE];
    char response[1024];
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
        if (strncmp(client->buffer, "AUTH ", 5) == 0) {
            if (sscanf(client->buffer, "AUTH %s %s %s", client->username, client->secret, displayName) == 3) {
                printf("Username: %s\n", client->username);
                printf("Secret: %s\n", client->secret);
                printf("Display name: %s\n", displayName);
                log_message("RECV", client->fd, client->buffer);
                snprintf(response, sizeof(response), "REPLY OK IS Auth success.\r\n");
                send(client->fd, response, strlen(response), 0);
                log_message("SENT", client->fd, response);
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

}

void handle_open_state(Client *client) {
    char command[10];

    while (client->state == OPEN_STATE) {
        int recv_len = recv(client->fd, client->buffer, BUFFER_SIZE - 1, 0);
        if (recv_len < 0) {
            perror("Error receiving data");
            client->state = ERROR_STATE;
            continue;
        } else if (recv_len == 0) {
            printf("Client disconnected.\n");
            client->state = END_STATE;
            break;
        }

        client->buffer[recv_len] = '\0';
        sscanf(client->buffer, "%s", command);

        if (strcmp(command, "JOIN") == 0) {
            char channelID[MAX_CHANNEL_ID_LENGTH];
            char displayName[MAX_DISPLAY_NAME_LENGTH];
            if (sscanf(client->buffer, "JOIN %s AS %s", channelID, displayName) == 2) {
                log_message("RECV", client->fd, client->buffer);
                if (join_channel(client, channelID) == 0) {
                    // Successfully joined
                    send(client->fd, "REPLY OK IS Joined channel\r\n", 28, 0);
                    log_message("SENT", client->fd, client->buffer);
                    broadcast_message(get_or_create_channel(client->channel), client->buffer, client);
                } else {
                    send(client->fd, "ERR Unable to join channel\r\n", 28, 0);
                }
            } else {
                send(client->fd, "ERR FROM Server IS Invalid JOIN format\r\n", 40, 0);
            }
        } else if (strcmp(command, "MSG") == 0) {
            char messageContent[BUFFER_SIZE];
            char fromDisplayName[MAX_DISPLAY_NAME_LENGTH];
            if (sscanf(client->buffer, "MSG FROM %s IS %[^\t\n]", fromDisplayName, messageContent) == 2) {
                log_message("RECV", client->fd, client->buffer);
            } else {
                send(client->fd, "ERR FROM Server IS Invalid MSG format\r\n", 38, 0);
            }
        } else if (strcmp(client->buffer, "BYE\r\n") == 0) {
            printf("Server received BYE\n");
            send(client->fd, "BYE\r\n", 5, 0);
            client->state = END_STATE;
        } else {
            send(client->fd, "ERR FROM Server IS Unknown command\r\n", 36, 0);
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
