#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <search.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <pthread.h>
#include <getopt.h>
#include "validation.h"
#include "cli.h"



typedef struct {
    int sockfd;
} thread_arg;




#define BUFFER_SIZE 1024
#define MAX_USERNAME_LENGTH 20
#define MAX_SECRET_LENGTH 128
#define MAX_DISPLAY_NAME_LENGTH 20
#define MAX_CHANNEL_ID_LENGTH 20
#define MAX_CHANNELS 10 // Maximum number of channels
#define MAX_CLIENTS 10 // Maximum number of simultaneous clients
#define POLL_TIMEOUT 20000 // Timeout for poll in milliseconds
#define DEFAULT_CHANNEL "default"

volatile sig_atomic_t serverRunning = 1;

// variables for handling the termination signal
bool terminateSignalReceived = false;
pthread_mutex_t terminateSignalMutex = PTHREAD_MUTEX_INITIALIZER;


void signalHandler(int signal) {
    if (signal == SIGINT) {
        pthread_mutex_lock(&terminateSignalMutex);
        terminateSignalReceived = true;
        pthread_mutex_unlock(&terminateSignalMutex);
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

    printf("%s %s:%d | %s", prefix, clientIP, clientPort, message);
}



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
    char displayName[MAX_DISPLAY_NAME_LENGTH];
} Client;


typedef struct {
    char channelName[MAX_CHANNEL_ID_LENGTH];
    Client *clients[MAX_CLIENTS];  // Pointers to clients in this channel
    int clientCount;
} Channel;

Channel channels[MAX_CHANNELS]; // Array to hold all channels
Client clients[MAX_CLIENTS];  // Global client list

bool allowMultipleConnections = false;  // Toggleable feature



Channel* find_channel_by_id(const char* channelID) {
    // Assuming there is a global array or list of channels
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (strcmp(channels[i].channelName, channelID) == 0) {
            // printf("Found channel by ID %s\n", channelID);
            return &channels[i];
        }
    }
    return NULL;
}


Channel* get_or_create_channel(const char* channelName) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (strcmp(channels[i].channelName, channelName) == 0) {
            return &channels[i]; // Channel exists
        }
    }

    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].channelName[0] == '\0') { // Empty slot
            strncpy(channels[i].channelName, channelName, MAX_CHANNEL_ID_LENGTH - 1);
            channels[i].clientCount = 0;
            printf("Created new channel %s\n", channelName);
            printf("All channels: %s\n", channels[i].channelName);
            printf("Clients in channel: %d\n", channels[i].clientCount);
            return &channels[i]; // Newly created channel
        }
    }

    return NULL;
}
void leave_channel(Client *client) {
    if (client->channel[0] == '\0') {
        printf("Client is not in any channel.\n");
        return; // Client is not in any channel
    }

    Channel* channel = find_channel_by_id(client->channel);
    if (!channel) {
        printf("No such channel found: %s\n", client->channel);
        return; // Channel does not exist
    }

    bool found = false;
    for (int i = 0; i < channel->clientCount; i++) {
        if (channel->clients[i] == client) {
            printf("Removing client %s from channel %s\n", client->username, channel->channelName);
            // Shift the rest of the clients down in the array
            memmove(&channel->clients[i], &channel->clients[i + 1], (channel->clientCount - i - 1) * sizeof(Client*));
            channel->clientCount--;
            memset(client->channel, 0, MAX_CHANNEL_ID_LENGTH);
            printf("Client %s left channel %s. New count: %d\n", client->username, channel->channelName, channel->clientCount);
            found = true;
            break;
        }
    }

    if (!found) {
        printf("Client %s not found in channel %s\n", client->username, client->channel);
    }
}


int join_channel(Client *client, const char* channelName) {
    if (strlen(channelName) >= MAX_CHANNEL_ID_LENGTH) {
        fprintf(stderr, "Error: channel name too long\n");
        return -1;
    }
    // Leave any previous channel
    leave_channel(client);

    Channel* channel = get_or_create_channel(channelName);
    if (!channel) {
        return -1; // Failed to create or find channel
    }

    if (channel->clientCount < MAX_CLIENTS) {
        channel->clients[channel->clientCount++] = client;

        strncpy(client->channel, channelName, MAX_CHANNEL_ID_LENGTH - 1);
        return 0;
    }

    return -1; // Channel is full
}



void broadcast_message(Channel *channel, const char *message, Client *sender) {
    // Check if the channel pointer is NULL before accessing its properties

    Channel *CHANNEL = find_channel_by_id(channel->channelName);
    if (!CHANNEL) {
        fprintf(stderr, "Channel %s not found.\n", channel->channelName);
        return;
    }

    printf("Broadcasting message to %d clients in channel %s\n", channel->clientCount, channel->channelName);

    for (int i = 0; i < channel->clientCount; i++) {
        Client *client = channel->clients[i];  // Get a pointer to the client

        // Check if the client pointer is NULL before using it
        if (!client) {
            fprintf(stderr, "Error: Null client in channel %s at index %d.\n", channel->channelName, i);
            continue;
        }

        // Skip the sender
        if (client == sender) {
            continue;
        }

        sleep(1);
        if (send(client->fd, message, strlen(message), 0) < 0) {
            perror("Error sending message");
        } else {
            printf ("Message %s sent to %s\n", message, client->username);
        }
    }
}



void handle_accept_state(Client *client) {
    char response[1024];
    char response2[1024];
    int recv_auth = 0;


    while (recv_auth == 0) {
        int recv_len = recv(client->fd, client->buffer, BUFFER_SIZE - 1, 0);
        if (recv_len < 0) {
            perror("Error receiving data");

        }


        client->buffer[recv_len] = '\0';
        if (strncmp(client->buffer, "AUTH ", 5) == 0) {
            int args_count = sscanf(client->buffer, "AUTH %s %s %s", client->username, client->secret, client->displayName);
            if (args_count == 3 && Check_username(client->username) && Check_secret(client->secret) && Check_Displayname(client->displayName)){
                log_message("RECV", client->fd, client->buffer);
                snprintf(response, sizeof(response), "REPLY OK IS Auth success.\r\n");
                send(client->fd, response, strlen(response), 0);
                sleep(1);
                log_message("SENT", client->fd, response);
                get_or_create_channel(DEFAULT_CHANNEL);
                join_channel(client, DEFAULT_CHANNEL);
                snprintf(response2, sizeof(response), "MSG FROM Server IS %s has joined %s.\r\n", client->displayName, DEFAULT_CHANNEL);
                send(client->fd, response2, strlen(response2), 0);
                log_message("SENT", client->fd, response2);
                sleep(1);
                broadcast_message(get_or_create_channel(DEFAULT_CHANNEL), response2, client);
                recv_auth = 1;
                client->state = OPEN_STATE;
            } else {
                log_message("RECV", client->fd, client->buffer);
                snprintf(response, sizeof(response), "REPLY NOK IS Auth success.\r\n");
                send(client->fd, response, strlen(response), 0);
                log_message("SENT", client->fd, response);
                recv_auth = 1;
                client->state = AUTH_STATE;
            }

        }


    }
}


void handle_auth_state(Client *client) {
    char response[1024];
    char response2[1024];
    int recv_auth = 0;
    while(recv_auth == 0){
        int recv_len = recv(client->fd, client->buffer, BUFFER_SIZE - 1, 0);
        if (recv_len < 0) {
            perror("Error receiving data");

        }

        client->buffer[recv_len] = '\0';
        if (strncmp(client->buffer, "AUTH ", 5) == 0) {
            int args_count = sscanf(client->buffer, "AUTH %s %s %s", client->username, client->secret, client->displayName);
            if (args_count == 3 && Check_username(client->username) && Check_secret(client->secret) && Check_Displayname(client->displayName)){
                log_message("RECV", client->fd, client->buffer);
                snprintf(response, sizeof(response), "REPLY OK IS Auth success.\r\n");
                send(client->fd, response, strlen(response), 0);
                sleep(1);
                log_message("SENT", client->fd, response);
                get_or_create_channel(DEFAULT_CHANNEL);
                join_channel(client, DEFAULT_CHANNEL);
                snprintf(response2, sizeof(response), "MSG FROM Server IS %s has joined %s.\r\n", client->displayName, DEFAULT_CHANNEL);
                send(client->fd, response2, strlen(response2), 0);
                log_message("SENT", client->fd, response2);
                sleep(1);
                broadcast_message(get_or_create_channel(DEFAULT_CHANNEL), response2, client);
                recv_auth = 1;
                client->state = OPEN_STATE;
            } else {
                log_message("RECV", client->fd, client->buffer);
                snprintf(response, sizeof(response), "REPLY NOK IS Auth success.\r\n");
                send(client->fd, response, strlen(response), 0);
                log_message("SENT", client->fd, response);
            }

        }


    }


}

void handle_open_state(Client *client) {
    char command[10];
    char response[1024];
    char messageContent[BUFFER_SIZE];
    char channelID[MAX_CHANNEL_ID_LENGTH];
    char displayName[MAX_DISPLAY_NAME_LENGTH];


    while (client->state == OPEN_STATE) {
        int recv_len = recv(client->fd, client->buffer, BUFFER_SIZE - 1, 0);
        if (recv_len < 0) {
            perror("Error receiving data");
            client->state = ERROR_STATE;
            continue;
        }

        client->buffer[recv_len] = '\0';
        sscanf(client->buffer, "%s", command);

        if (strcmp(command, "JOIN") == 0) {
            if (sscanf(client->buffer, "JOIN %s AS %s", channelID, displayName) == 2) {

                if (client->channel[0] != '\0') {
                    leave_channel(client);
                }
                strncpy(client->channel, channelID, MAX_CHANNEL_ID_LENGTH - 1);
                client->channel[MAX_CHANNEL_ID_LENGTH - 1] = '\0';
                strncpy(client->displayName, displayName, MAX_DISPLAY_NAME_LENGTH - 1);
                client->displayName[MAX_DISPLAY_NAME_LENGTH - 1] = '\0';

                    log_message("RECV", client->fd, client->buffer);
                    send(client->fd, "REPLY OK IS Join success.\r\n", 28, 0);
                    sleep(1);
                    log_message("SENT", client->fd, client->buffer);
                    snprintf(response, sizeof(response), "MSG FROM Server IS %s has joined %s.\r\n", client->displayName, client->channel);
                    send (client->fd, response, strlen(response), 0);
                    sleep(1);
                    log_message("SENT", client->fd, response);



                char currentChannel[MAX_CHANNEL_ID_LENGTH];
                strncpy(currentChannel, client->channel, MAX_CHANNEL_ID_LENGTH);
                if (join_channel(client, currentChannel) != 0) {
                    fprintf(stderr, "Error joining channel\n");
                }
                    broadcast_message(get_or_create_channel(client->channel), response, client);
                sleep(1);
            } else {
                send(client->fd, "ERR FROM Server IS Invalid JOIN format\r\n", 40, 0);
                sleep(1);
            }
        } else if (strcmp(command, "MSG") == 0) {
            if (sscanf(client->buffer, "MSG FROM %s IS %[^\r\n]", client->displayName, messageContent) == 2) {
                log_message("RECV", client->fd, client->buffer);
                broadcast_message(get_or_create_channel(client->channel), client->buffer, client);
                sleep(1);
            } else {
                send(client->fd, "ERR FROM Server IS Invalid MSG format\r\n", 38, 0);
                sleep(1);
            }
        } else if (strcmp(client->buffer, "BYE\r\n") == 0) {
            printf("Server received BYE\n");
            send(client->fd, "BYE\r\n", 5, 0);
            client->state = END_STATE;
        }  else if(strcmp(command, "ERR") == 0){
            if ((sscanf(client->buffer, "ERR FROM %s IS %[^\r\n]", client->displayName, messageContent) == 2)){
                log_message("RECV", client->fd, client->buffer);
                client->state = ERROR_STATE;
            }else{
                printf("ERROR IN ERR COMMAND FROM CLIENT\n");
            }

        }
        else {
            log_message("RECV", client->fd, client->buffer);
            printf("Invalid command FROM CLIENT\n");
            client->state = ERROR_STATE;

        }
    }
}


void handle_error_state(Client *client) {
    const char* byeMsg = "BYE\r\n";
    send(client->fd, byeMsg, strlen(byeMsg), 0);
    client->state = END_STATE;
}

void handle_end_state(Client *client) {

    close(client->fd);
    client->fd = -1;
    exit (0);
}


void* client_handler(void* arg) {
    thread_arg* t_arg = (thread_arg*) arg;
    int client_sock = t_arg->sockfd;
    free(arg);

    Client client;
    client.fd = client_sock;
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
    return NULL;
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
        struct sockaddr_in cli_addr;
        socklen_t clilen = sizeof(cli_addr);
        int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) {
            perror("ERROR on accept");
            if (errno == EINTR)
                break;
            continue;
        }

        pthread_t thread_id;
        thread_arg *arg = malloc(sizeof(thread_arg));
        if (arg == NULL) {
            perror("Failed to allocate memory for thread arg");
            close(newsockfd);
            continue;
        }
        arg->sockfd = newsockfd;

        if (pthread_create(&thread_id, NULL, client_handler, (void*) arg) != 0) {
            perror("Failed to create thread");
            free(arg);
            close(newsockfd);
        }
        pthread_detach(thread_id);
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
    if (config.server_ip[0] == '\0') {
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


