#include "tcp_client_handler.h"
#include "channels.h"
#include "validation.h"
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
            leave_channel(client);
            close(client->fd);
            client->fd = -1;
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