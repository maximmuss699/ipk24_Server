#include "validation.h"
#include "cli.h"
#include "channels.h"
#include "client.h"
#include "tcp_client_handler.h"



typedef struct {
    int sockfd;
    int protocol;
    struct sockaddr_in addr; // Client address
    socklen_t addr_len;      // Length of client address
} thread_arg;



typedef struct {
    State state;                        // Current state of the client
    char buffer[BUFFER_SIZE];           // Buffer to hold incoming data
    char username[MAX_USERNAME_LENGTH]; // Username for the client
    char channel[MAX_CHANNEL_ID_LENGTH];// Channel the client is subscribed to
    char secret[MAX_SECRET_LENGTH];     // Secret/password for authentication
    char displayName[MAX_DISPLAY_NAME_LENGTH]; // Display name of the client
    struct sockaddr_in addr;            // Client's address
    socklen_t addr_len;                 // Length of the client's address
    int sockfd;                         // Socket file descriptor
    pthread_t thread_id;                // Thread ID for the client
} UDPClient;


#define MAX_UDP_CLIENTS 100
UDPClient udpClients[MAX_UDP_CLIENTS];
int udpClientCount = 0;
pthread_mutex_t udpClientMutex = PTHREAD_MUTEX_INITIALIZER;


volatile sig_atomic_t serverRunning = 1;
uint16_t messageID = 0x0000;

void cleanup_udp_sessions(void) {
    pthread_mutex_lock(&udpClientMutex);
    // Perform cleanup
    pthread_mutex_unlock(&udpClientMutex);
}

void signalHandler(int signum) {
    serverRunning = 0;
    cleanup_udp_sessions();
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

    uint16_t netOrderMessageID = htons(message_id);
    buffer[offset++] = 0x00;  // Message type for CONFIRM
    buffer[offset++] = (char)((netOrderMessageID >> 8) & 0xFF);
    buffer[offset++] = (char)(netOrderMessageID & 0xFF);

    // Отправляем сообщение
    if (sendto(sockfd, buffer, offset, 0, (struct sockaddr *)cli_addr, cli_len) < 0) {
        perror("Failed to send confirm");
    } else {
        printf("Confirm sent successfully in state \n");
    }


}


void handle_udp_accept_state(UDPClient *client) {
    struct pollfd fds;
    fds.fd = client->sockfd;
    fds.events = POLLIN;

    while(client->state == ACCEPT_STATE) {
        int ret = poll(&fds, 1, POLL_TIMEOUT);
        if (ret < 0) {
            perror("Poll failed");
            client->state = ERROR_STATE;
            return;
        } else if (ret == 0) {
            printf("Timeout occurred, no data received.\n");
            continue;
        }

        if (fds.revents & POLLIN) {
            memset(client->buffer, 0, BUFFER_SIZE);
            int bytes_received = recvfrom(client->sockfd, client->buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client->addr, &client->addr_len);
            if (bytes_received < 0) {
                perror("ACCEPT recvfrom failed");
                client->state = ERROR_STATE;
                return;
            }

            uint16_t receivedMessageID = (client->buffer[1] << 8) | client->buffer[2];
            send_confirm(client->sockfd, &client->addr, client->addr_len, receivedMessageID);

            size_t offset1 = 3;  // Adjusting the starting position after message type and ID
            char username[100], display_name[100], secret[100];
            strcpy(username, client->buffer + offset1);
            offset1 += strlen(username) + 1;
            strcpy(display_name, client->buffer + offset1);
            offset1 += strlen(display_name) + 1;
            strcpy(secret, client->buffer + offset1);

            if (client->buffer[0] == 0x02) {
                if (Check_username(username) && Check_secret(secret) && Check_Displayname(display_name)) {
                    strcpy(client->username, username);
                    strcpy(client->secret, secret);
                    strcpy(client->displayName, display_name);
                    printf("Transitioning from ACCEPT_STATE to OPEN_STATE\n");
                    client->state = OPEN_STATE;
                    send_reply(client->sockfd, &client->addr, client->addr_len, 1, "Authentication successful", receivedMessageID);
                } else {
                    client->state = ERROR_STATE;
                    char response[] = "Authentication failed";
                    sendto(client->sockfd, response, strlen(response), 0, (struct sockaddr *)&client->addr, client->addr_len);
                }
            } else {
                char response[] = "Authentication failed";
                sendto(client->sockfd, response, strlen(response), 0, (struct sockaddr *)&client->addr, client->addr_len);
            }
        }
    }
}

void handle_udp_open_state(UDPClient *client){
    struct pollfd fds;
    fds.fd = client->sockfd;
    fds.events = POLLIN;

    while(client->state == OPEN_STATE) {
        printf("OPEN STATE\n");

        int ret = poll(&fds, 1, POLL_TIMEOUT);
        if (ret < 0) {
            perror("Poll failed");
            client->state = ERROR_STATE;
            return;
        } else if (ret == 0) {
            printf("Timeout occurred, no data received.\n");
            continue;
        }

        if (fds.revents & POLLIN) {  // If there is data to read
            memset(client->buffer, 0, BUFFER_SIZE);
            int bytes_received = recvfrom(client->sockfd, client->buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client->addr, &client->addr_len);
            if (bytes_received < 0) {
                perror("recvfrom failed");
                client->state = ERROR_STATE;
                return;
            }

            uint16_t receivedMessageID = ntohs(*(uint16_t *)(client->buffer + 1));
            uint8_t messageType = client->buffer[0];
            char *displayName = client->buffer + 3;
            char *messageContent = displayName + strlen(displayName) + 1;
            printf("State is %d\n", client->state);
            send_confirm(client->sockfd, &client->addr, client->addr_len, receivedMessageID);

            switch (messageType) {
                case 0x03:  // JOIN
                    printf("JOIN\n");
                    break;

                case 0x04:  // MSG
                case 0xFE:  // ERR
                    printf("%s: %s\n", displayName, messageContent);
                    // broadcast_message(client->channel, messageContent, displayName, messageType);
                    break;

                default:
                    printf("Unknown message type received: %d\n", messageType);
                    //send_error_reply(client->sockfd, &client->addr, client->addr_len, "Invalid message type", receivedMessageID);
                    break;
            }
        }
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

    while (client.state != END_STATE  && serverRunning) {

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
            // After processing, check for shutdown signal
            if (!serverRunning) {
                break; // Break the loop to start cleanup
            }

    }
    close(client.fd);
    return NULL;
}


void* udp_client_handler(void* arg) {
    UDPClient *client = (UDPClient *)arg;
    printf("UDP client handler started\n");
    printf("Client state: %d\n", client->state);
    printf("Client buffer: %s\n", client->buffer);

    while (client->state != END_STATE && serverRunning) {
        switch (client->state) {
            case ACCEPT_STATE:
                handle_udp_accept_state(client);
                break;
            case AUTH_STATE:
               // handle_auth_state(client);  // Assuming UDP can use a similar auth handler
                break;
            case OPEN_STATE:
                handle_udp_open_state(client);
                break;
            case ERROR_STATE:
              //  handle_error_state(client);
                break;
            case END_STATE:
             //   handle_end_state(client);
                break;
            default:
                printf("Unknown UDP state\n");
                client->state = END_STATE;
        }
        if (!serverRunning) {
            break;  // Break the loop to start cleanup
        }
    }
    close(client->sockfd);
    client->sockfd = -1;
    free(client);  // Ensure to free the allocated memory for the client
    return NULL;
}



int find_or_create_udp_session(int udp_sockfd, struct sockaddr_in *addr, socklen_t len) {
    pthread_mutex_lock(&udpClientMutex);
    for (int i = 0; i < udpClientCount; i++) {
        if (udpClients[i].addr_len == len && memcmp(&udpClients[i].addr, addr, len) == 0) {
            return i; // Session found
        }
    }

    // Create new session if possible
    if (udpClientCount < MAX_UDP_CLIENTS) {
        UDPClient *client = &udpClients[udpClientCount++];
        client->addr = *addr;
        client->addr_len = len;
        client->state = ACCEPT_STATE;
        client->sockfd = udp_sockfd;  // Storing the socket descriptor if needed for sending
        pthread_mutex_unlock(&udpClientMutex);
        return udpClientCount - 1;
    }
    pthread_mutex_unlock(&udpClientMutex);

    return -1; // No space for more clients
}

void handle_udp_packet(int udp_sockfd) {
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    char buffer[BUFFER_SIZE];
    int bytes_received = recvfrom(udp_sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&cli_addr, &cli_len);
    if (bytes_received < 0) {
        perror("recvfrom failed");
        return;
    }
    printf("Received UDP packet from %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));


    // Check if this client is known
    int clientIndex = find_or_create_udp_session(udp_sockfd, &cli_addr, cli_len);
    if (clientIndex < 0) {
        fprintf(stderr, "Failed to handle new UDP client");
        return;
    }
    UDPClient *client = &udpClients[clientIndex];
    memcpy(client->buffer, buffer, bytes_received);

    // If new session, start a new thread
    if (pthread_equal(client->thread_id, pthread_self()) || client->thread_id == 0) {
        // If no thread is running, start a new thread
        if (pthread_create(&client->thread_id, NULL, udp_client_handler, client) != 0) {
            perror("Failed to create thread for new UDP client");
        } else {
            pthread_detach(client->thread_id);
        }
    } else {
        // If a thread is already running, update data (could use a condition variable or another synchronization mechanism here)
        // Signal to the existing thread that new data has arrived (not implemented here)
        printf("Thread already running for this client. Updated data in buffer.\n");
    }
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
        FD_SET(sockfd, &readfds);
        FD_SET(udp_sockfd, &readfds);

        max_fd = (sockfd > udp_sockfd) ? sockfd : udp_sockfd;

        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            printf("Select error");
        }

        if (!serverRunning) {
            break; // Stop the loop if shutdown is signaled
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
            handle_udp_packet(udp_sockfd);

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


