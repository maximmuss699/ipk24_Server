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
#include "validation.h"
#include "cli.h"
#include "channels.h"
#include "client.h"



typedef struct {
    int sockfd;
    int protocol;
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

uint16_t messageID = 0x0000;

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


bool allowMultipleConnections = false;  // Toggleable feature




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

void send_reply(int sockfd, struct sockaddr_in *cli_addr, socklen_t cli_len, uint8_t result, const char *message_contents, uint16_t ref_message_id) {
    unsigned char buffer[1024];
    int offset = 0;
    int recv_confirm = 0;



    uint16_t netOrderMessageID = htons(messageID);
    buffer[offset++] = 0x01;  // Message type for REPLY
    buffer[offset++] = (char)((netOrderMessageID >> 8) & 0xFF);
    buffer[offset++] = (char)(netOrderMessageID & 0xFF);

    // Результат операции
    buffer[offset++] = result;


    // ID сообщения, на которое мы отвечаем
    uint16_t net_ref_message_id = htons(ref_message_id);
    buffer[offset++] = (net_ref_message_id >> 8) & 0xFF;
    buffer[offset++] = net_ref_message_id & 0xFF;

    // Содержимое сообщения
    int message_len = strlen(message_contents);
    memcpy(buffer + offset, message_contents, message_len);
    offset += message_len;

    // Null-terminated string
    buffer[offset++] = 0;

    // Отправляем сообщение
    if (sendto(sockfd, buffer, offset, 0, (struct sockaddr *)cli_addr, cli_len) < 0) {
        perror("Failed to send reply");
    } else {
        printf("Reply sent successfully\n");
    }
    while(recv_confirm == 0){
        int recv_len = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        if (recv_len < 0) {
            perror("Error receiving data");

        }

        buffer[recv_len] = '\0';
        if (buffer[0] == 0x00) {
            uint16_t receivedMessageID = (buffer[1] << 8) | buffer[2];
            if (receivedMessageID == messageID) {
                recv_confirm = 1;
            }
        }
    }

}

void send_confirm(int sockfd, struct sockaddr_in *cli_addr, socklen_t cli_len, uint16_t message_id) {
    unsigned char buffer[1024];
    int offset = 0;

    uint16_t netOrderMessageID = htons(messageID);
    buffer[offset++] = 0x00;  // Message type for CONFIRM
    buffer[offset++] = (char)((netOrderMessageID >> 8) & 0xFF);
    buffer[offset++] = (char)(netOrderMessageID & 0xFF);

    // Отправляем сообщение
    if (sendto(sockfd, buffer, offset, 0, (struct sockaddr *)cli_addr, cli_len) < 0) {
        perror("Failed to send confirm");
    } else {
        printf("Confirm sent successfully\n");
    }


}



void handle_udp_accept_state(Client *client) {
    char buffer[1024];
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    int bytes_received;

    while(client->state == ACCEPT_STATE) {
        bytes_received = recvfrom(client->fd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, &cli_len);
        if (bytes_received < 0) {
            perror("recvfrom failed");
            client->state = ERROR_STATE;
            return;
        }
        uint16_t receivedMessageID = (buffer[1] << 8) | buffer[2];

        send_confirm(client->fd, &cli_addr, cli_len, receivedMessageID);


        size_t offset = 0;
        size_t offset1 = 0;
        uint8_t message_type = buffer[offset++];
        offset1 += 2;

        char username[100], display_name[100], secret[100];
        int current_pos = 0;
        strcpy(username, buffer + offset1);
        current_pos = strlen(username) + 1;
        offset1 += current_pos;


        strcpy(display_name, buffer + offset1);
        current_pos = strlen(display_name) + 1;
        offset1 += current_pos;
        strcpy(secret, buffer + offset1);

        if (buffer[0] == 0x02) {
        if (Check_username(username) && Check_secret(secret) && Check_Displayname(display_name)) {
            strcpy(client->username, username);
            strcpy(client->secret, secret);
            strcpy(client->displayName, display_name);
            client->state = OPEN_STATE;
            send_reply(client->fd, &cli_addr, cli_len, 1, "Authentication successful", receivedMessageID);


        } else {
            client->state = ERROR_STATE;
            char response[] = "Authentication failed";
            sendto(client->fd, response, strlen(response), 0, (struct sockaddr *) &cli_addr, cli_len);
        }
    }else{
            client->state = ERROR_STATE;
            char response[] = "Authentication failed";
            sendto(client->fd, response, strlen(response), 0, (struct sockaddr *) &cli_addr, cli_len);

        }
    }
}

void handle_udp_open_state(Client *client){
    char buffer[1024];
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    int bytes_received;

    while(client->state == OPEN_STATE) {
        bytes_received = recvfrom(client->fd, buffer, sizeof(buffer), 0, (struct sockaddr *) &cli_addr, &cli_len);
        if (bytes_received < 0) {
            perror("recvfrom failed");
            client->state = ERROR_STATE;
            return;
        }
        uint16_t receivedMessageID = (buffer[1] << 8) | buffer[2];

        send_confirm(client->fd, &cli_addr, cli_len, receivedMessageID);

        size_t offset = 0;
        size_t offset1 = 0;
        uint8_t message_type = buffer[offset++];
        offset1 += 2;

        char username[100], display_name[100], secret[100];
        int current_pos = 0;
        strcpy(username, buffer + offset1);
        current_pos = strlen(username) + 1;
        offset1 += current_pos;
    }

}



void* client_handler(void* arg) {
    thread_arg* t_arg = (thread_arg*) arg;
    int client_sock = t_arg->sockfd;
    int protocol = t_arg->protocol;
    free(arg);

    Client client;
    client.fd = client_sock;
    client.state = ACCEPT_STATE;
    memset(client.buffer, 0, BUFFER_SIZE);

    while (client.state != END_STATE) {
        if (protocol == 0) {  // TCP
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
                    printf("Unknown TCP state\n");
                    client.state = END_STATE;
            }
        } else if (protocol == 1) {  // UDP
            switch (client.state) {
                case ACCEPT_STATE:
                    handle_udp_accept_state(&client);
                    break;
                case AUTH_STATE:
                  //  handle_udp_auth_state(&client);
                    printf("UDP AUTH STATE\n");
                    close(client.fd);
                    exit(1);
                    break;
                case OPEN_STATE:
                    handle_udp_open_state(&client);
                    break;
                case ERROR_STATE:
                    //handle_udp_error_state(&client);
                    break;
                case END_STATE:
                    //handle_udp_end_state(&client);
                    break;
                default:
                    printf("Unknown UDP state\n");
                    client.state = END_STATE;
            }
        }
    }
    close(client.fd);
    return NULL;
}




void FSM_function(void) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening TCP socket");
        exit(1);
    }
    int udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sockfd < 0) {
        perror("ERROR opening UDP socket");
        exit(1);
    }


    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(config.server_ip);
    serv_addr.sin_port = htons(config.server_port);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }
    if (bind(udp_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding UDP");
        close(udp_sockfd);
        exit(1);
    }

    listen(sockfd, MAX_CLIENTS);
    fd_set readfds;
    int max_fd;

    while (serverRunning) {

        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds); // Для TCP
        FD_SET(udp_sockfd, &readfds); // Для UDP

        max_fd = (sockfd > udp_sockfd) ? sockfd : udp_sockfd;

        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            printf("Select error");
        }


        if (FD_ISSET(sockfd, &readfds)) {
            int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
            if (newsockfd < 0) {
                perror("ERROR on accept");
                continue;
            }
            pthread_t thread_id;
            thread_arg *arg = malloc(sizeof(thread_arg));
            arg->sockfd = newsockfd;
            arg->protocol = 0; // TCP
            if (pthread_create(&thread_id, NULL, client_handler, (void*) arg) != 0) {
                perror("Failed to create thread");
                free(arg);
                close(newsockfd);
            }
            pthread_detach(thread_id);
        }

        if (FD_ISSET(udp_sockfd, &readfds)) {

            pthread_t thread_id;
            thread_arg *arg = malloc(sizeof(thread_arg));
            arg->sockfd = udp_sockfd;
            arg->protocol = 1; // UDP


            if (pthread_create(&thread_id, NULL, client_handler, (void*) arg) != 0) {
                perror("Failed to create thread");
                free(arg);
                close(udp_sockfd);
            }
            pthread_detach(thread_id);
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


