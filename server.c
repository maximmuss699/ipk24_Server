/**
 * @file IPK24-CHAT.c
 *
 * @name IPK project 2 - Server for chat application
 * @brief Server for chat application
 * @author Maksim Samusevich(xsamus00)
 * @date 22.04.2024
 */

#include "validation.h"
#include "cli.h"
#include "channels.h"
#include "client.h"
#include "tcp_client_handler.h"



typedef struct {
    int sockfd; // Socket file descriptor
    int protocol; // 0 for TCP, 1 for UDP
    struct sockaddr_in addr; // Client address
    socklen_t addr_len;      // Length of client address
} thread_arg;


#define MAX_UDP_CLIENTS 100
#define MAX_MESSAGE_LENGTH 1024
Client udpClients[MAX_UDP_CLIENTS]; // Array of UDP clients
int udpClientCount = 0; // Number of active UDP clients
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

// Send a reply message to the UDP client
void send_reply(int sockfd, struct sockaddr_in *cli_addr, socklen_t cli_len, uint8_t result, const char *message_contents, uint16_t ref_message_id) {
    unsigned char buffer[1024];
    int offset = 0;
    int recv_confirm = 0;


    uint16_t netOrderMessageID = htons(messageID);
    buffer[offset++] = 0x01;  // Message type for REPLY
    buffer[offset++] = (char)((netOrderMessageID >> 8) & 0xFF);
    buffer[offset++] = (char)(netOrderMessageID & 0xFF);


    buffer[offset++] = result;

    uint16_t net_ref_message_id = htons(ref_message_id);
    buffer[offset++] = (net_ref_message_id >> 8) & 0xFF;
    buffer[offset++] = net_ref_message_id & 0xFF;

    // Copy the message contents to the buffer
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
    while(recv_confirm == 0) // Wait for confirm message
    {
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

// Send a confirm message to the UDP client
void send_confirm(int sockfd, struct sockaddr_in *cli_addr, socklen_t cli_len, uint16_t message_id) {
    unsigned char buffer[1024];
    int offset = 0;

    uint16_t netOrderMessageID = htons(message_id);
    buffer[offset++] = 0x00;  // Message type for CONFIRM
    buffer[offset++] = (char)((netOrderMessageID >> 8) & 0xFF);
    buffer[offset++] = (char)(netOrderMessageID & 0xFF);

    // Send the confirm message
    if (sendto(sockfd, buffer, offset, 0, (struct sockaddr *)cli_addr, cli_len) < 0) {
        perror("Failed to send confirm");
    } else {
        printf("Confirm sent successfully in state \n");
    }


}

// Send a MSG message to the UDP client
void send_msg(int sockfd, const struct sockaddr_in *addr, socklen_t addr_len, const char *displayName, const char *channel, uint16_t messageID) {
    char buffer[MAX_MESSAGE_LENGTH];
    int offset = 0;
    int recv_confirm = 0;
    // Message type for MSG
    buffer[offset++] = 0x04;

    uint16_t netOrderMessageID = htons(messageID);
    buffer[offset++] = (netOrderMessageID >> 8) & 0xFF; // High byte
    buffer[offset++] = netOrderMessageID & 0xFF;        // Low byte

    // Static part of the message as "Server"
    const char *serverName = "Server";
    strcpy(buffer + offset, serverName);
    offset += strlen(serverName) + 1; // +1 for null terminator

    // Construct the message content and add a null terminator
    snprintf(buffer + offset, MAX_MESSAGE_LENGTH - offset, "%s has joined %s", displayName, channel);
    offset += strlen(buffer + offset) + 1; // Move offset past the end of the string including null terminator

    // Ensure the total message does not exceed buffer size
    if (offset > MAX_MESSAGE_LENGTH) {
        fprintf(stderr, "Error: Message too long to send\n");
        return;
    }

    // Send the packet
    ssize_t sentBytes = sendto(sockfd, buffer, offset, 0, (const struct sockaddr *)addr, addr_len);
    if (sentBytes < 0) {
        perror("Failed to send message");
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


void handle_udp_accept_state(Client *client) {
    char response[1024];
    char response2[1024];
    if (client->state == ACCEPT_STATE) {
        // Assume buffer is already populated before this function is called
        uint16_t receivedMessageID = (client->buffer[1] << 8) | client->buffer[2];
        send_confirm(client->fd, &client->addr, client->addr_len, receivedMessageID);

        size_t offset1 = 3; // Adjust for message type and message ID
        char username[100], display_name[100], secret[100];
        strcpy(username, client->buffer + offset1);
        offset1 += strlen(username) + 1;
        strcpy(display_name, client->buffer + offset1);
        offset1 += strlen(display_name) + 1;
        strcpy(secret, client->buffer + offset1);

        if (client->buffer[0] == 0x02) { // Check if it's an AUTH message
            if (Check_username(username) && Check_secret(secret) && Check_Displayname(display_name)) {
                strcpy(client->username, username);
                strcpy(client->secret, secret);
                strcpy(client->displayName, display_name);
                printf("Authentication successful, transitioning to OPEN_STATE\n");
                client->state = OPEN_STATE;
                get_or_create_channel(DEFAULT_CHANNEL);
                join_channel(client, DEFAULT_CHANNEL);
                send_reply(client->fd, &client->addr, client->addr_len, 1, "Authentication successful", receivedMessageID);
                sleep(1);
                send_msg(client->fd, &client->addr, client->addr_len, client->displayName, DEFAULT_CHANNEL, messageID);
                snprintf(response2, sizeof(response), "MSG FROM Server IS %s has joined %s.\r\n", client->displayName, DEFAULT_CHANNEL);
                send(client->fd, response2, strlen(response2), 0);
                broadcast_message(get_or_create_channel(DEFAULT_CHANNEL), response2, client);
            } else {
                printf("Authentication failed\n");
                client->state = ERROR_STATE;
                char response[] = "Authentication failed";
                sendto(client->fd, response, strlen(response), 0, (struct sockaddr *)&client->addr, client->addr_len);
            }
        } else {
            printf("Received unexpected message type\n");
            client->state = ERROR_STATE;
            char response[] = "Invalid message type";
            sendto(client->fd, response, strlen(response), 0, (struct sockaddr *)&client->addr, client->addr_len);
        }
    }
}


void handle_udp_open_state(Client *client){
    struct pollfd fds;
    fds.fd = client->fd;
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
            int bytes_received = recvfrom(client->fd, client->buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client->addr, &client->addr_len);
            if (bytes_received < 0) {
                perror("recvfrom failed");
                client->state = ERROR_STATE;
                return;
            }

            uint16_t receivedMessageID = ntohs(*(uint16_t *)(client->buffer + 1));
            uint8_t messageType = client->buffer[0];
           // char *displayName = client->buffer + 3;
           // char *messageContent = displayName + strlen(displayName) + 1;
            printf("State is %d\n", client->state);
            send_confirm(client->fd, &client->addr, client->addr_len, receivedMessageID);

            switch (messageType) {
                case 0x03:  // JOIN

                    break;
                case 0x04:  // MSG
                    break;
                case 0xFE:  // ERR

                    break;
                default:

                    break;
            }
        }
    }
}



// Handler for TCP clients
void* client_handler(void* arg)
{
    // Creating a thread for each client
    thread_arg* t_arg = (thread_arg*) arg;
    int client_sock = t_arg->sockfd;
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


void* udp_client_handler( Client *client){
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
    close(client->fd);
    client->fd = -1;

    return NULL;
}


// Find or create a UDP session
int find_or_create_udp_session(struct sockaddr_in *addr, socklen_t len, int udp_sockfd) {
    for (int i = 0; i < udpClientCount; i++) {
        if (udpClients[i].active && memcmp(&udpClients[i].addr, addr, sizeof(struct sockaddr_in)) == 0)
        {
            return i; // Session found
        }
    }

    if (udpClientCount < MAX_UDP_CLIENTS) {
        Client *client = &udpClients[udpClientCount];
        client->addr = *addr;
        client->addr_len = len;
        client->fd = udp_sockfd;
        client->active = 1;
        client->state = ACCEPT_STATE;  // Set initial state to ACCEPT_STATE
        memset(client->buffer, 0, BUFFER_SIZE); // Initialize buffer
        // Initialize other fields as necessary
        memset(client->username, 0, MAX_USERNAME_LENGTH);
        memset(client->channel, 0, MAX_CHANNEL_ID_LENGTH);
        memset(client->secret, 0, MAX_SECRET_LENGTH);
        memset(client->displayName, 0, MAX_DISPLAY_NAME_LENGTH);

        udpClientCount++;
        return udpClientCount - 1; // Return the new client's index
    }

    return -1; // No space for more clients
}


// Handle incoming UDP packets
void handle_udp_packet(int udp_sockfd) {
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    char buffer[BUFFER_SIZE];

    struct pollfd fds;
    fds.fd = udp_sockfd;
    fds.events = POLLIN;
    int ret = poll(&fds, 1, POLL_TIMEOUT);

    if (ret < 0) {
        perror("Poll failed");
        return;
    } else if (ret == 0) {
        printf("Poll timeout, no data received.\n");
        return;
    }

    if (fds.revents & POLLIN) {
        int bytes_received = recvfrom(udp_sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cli_addr, &cli_len);
        if (bytes_received < 0) {
            perror("recvfrom failed");
            return;
        }

        int session_index = find_or_create_udp_session(&cli_addr, cli_len, udp_sockfd);
        if (session_index >= 0) {
            memcpy(udpClients[session_index].buffer, buffer, bytes_received);
            printf("Received UDP packet from %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

            udp_client_handler(&udpClients[session_index]);

        } else {
            printf("Failed to handle new UDP client or no space left.\n");
        }
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


