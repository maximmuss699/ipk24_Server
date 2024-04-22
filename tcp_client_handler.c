#include "tcp_client_handler.h"
#include "channels.h"
#include "validation.h"
#define MAX_MESSAGE_LENGTH 4096


void handle_accept_state(Client *client) {
    static char accumulated_data[MAX_MESSAGE_LENGTH]; // Buffer to accumulate data
    static int accumulated_length = 0; // Current length of accumulated data

    char response[1024];
    char response2[1024];
    int recv_auth = 0;
    char *end_of_message;


    while (recv_auth == 0) {
        int recv_len = recv(client->fd, client->buffer, BUFFER_SIZE - 1, 0);
        if (recv_len < 0) {
            perror("Error receiving data");

        }
        client->buffer[recv_len] = '\0';

        if (accumulated_length + recv_len < MAX_MESSAGE_LENGTH) {
            strcat(accumulated_data, client->buffer); // Append new data
            accumulated_length += recv_len;
        }

        end_of_message = strstr(accumulated_data, "\r\n");
        if (end_of_message) {
            *end_of_message = '\0';
            strcat(accumulated_data, "\n");
            strncpy(client->buffer, accumulated_data, BUFFER_SIZE - 1);
            client->buffer[BUFFER_SIZE - 1] = '\0';
            memset(accumulated_data, 0, MAX_MESSAGE_LENGTH);  // Clear accumulated_data to prevent old data interference
            accumulated_length = 0;
            if (strncmp(client->buffer, "AUTH ", 5) == 0) {
                int args_count = sscanf(client->buffer, "AUTH %s %s %s", client->username, client->secret,
                                        client->displayName);
                if (args_count == 3 && Check_username(client->username) && Check_secret(client->secret) &&
                    Check_Displayname(client->displayName)) {
                    log_message("RECV", client->fd, client->buffer, "AUTH");
                    snprintf(response, sizeof(response), "REPLY OK IS Auth success.\r\n");
                    send(client->fd, response, strlen(response), 0);
                    sleep(1);
                    log_message("SENT", client->fd, response, "REPLY");
                    get_or_create_channel(DEFAULT_CHANNEL);
                    join_channel(client, DEFAULT_CHANNEL);
                    snprintf(response2, sizeof(response), "MSG FROM Server IS %s has joined %s.\r\n", client->displayName, DEFAULT_CHANNEL);
                    send(client->fd, response2, strlen(response2), 0);
                    log_message("SENT", client->fd, response2, "MSG");
                    sleep(1);
                    broadcast_message(get_or_create_channel(DEFAULT_CHANNEL), response2, client);
                    recv_auth = 1;
                    client->state = OPEN_STATE;
                } else {
                    log_message("RECV", client->fd, client->buffer, "AUTH");
                    snprintf(response, sizeof(response), "REPLY NOK IS Auth success.\r\n");
                    send(client->fd, response, strlen(response), 0);
                    log_message("SENT", client->fd, response, "REPLY");
                    recv_auth = 1;
                    client->state = AUTH_STATE;
                }

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
                log_message("RECV", client->fd, client->buffer, "AUTH");
                snprintf(response, sizeof(response), "REPLY OK IS Auth success.\r\n");
                send(client->fd, response, strlen(response), 0);
                sleep(1);
                log_message("SENT", client->fd, response, "REPLY");
                get_or_create_channel(DEFAULT_CHANNEL);
                join_channel(client, DEFAULT_CHANNEL);
                snprintf(response2, sizeof(response), "MSG FROM Server IS %s has joined %s.\r\n", client->displayName, DEFAULT_CHANNEL);
                send(client->fd, response2, strlen(response2), 0);
                log_message("SENT", client->fd, response2, "MSG");
                sleep(1);
                broadcast_message(get_or_create_channel(DEFAULT_CHANNEL), response2, client);
                recv_auth = 1;
                client->state = OPEN_STATE;
            } else {
                log_message("RECV", client->fd, client->buffer, "AUTH");
                snprintf(response, sizeof(response), "REPLY NOK IS Auth success.\r\n");
                send(client->fd, response, strlen(response), 0);
                log_message("SENT", client->fd, response, "REPLY");
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
    char previousChannel[MAX_CHANNEL_ID_LENGTH];
    char left_message[1024];


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
                strncpy(previousChannel, client->channel, MAX_CHANNEL_ID_LENGTH - 1);
                previousChannel[MAX_CHANNEL_ID_LENGTH - 1] = '\0';
                snprintf(left_message, sizeof(left_message), "MSG FROM Server IS %s has left %s.\r\n", client->displayName, previousChannel);
                if (client->channel[0] != '\0') {
                    leave_channel(client);
                }
                broadcast_message(get_or_create_channel(previousChannel), left_message, client);
                strncpy(client->channel, channelID, MAX_CHANNEL_ID_LENGTH - 1);
                client->channel[MAX_CHANNEL_ID_LENGTH - 1] = '\0';
                strncpy(client->displayName, displayName, MAX_DISPLAY_NAME_LENGTH - 1);
                client->displayName[MAX_DISPLAY_NAME_LENGTH - 1] = '\0';

                log_message("RECV", client->fd, client->buffer, "JOIN");
                send(client->fd, "REPLY OK IS Join success.\r\n", 28, 0);
                sleep(1);
                log_message("SENT", client->fd, "REPLY OK IS Join success.\r\n", "REPLY");
                snprintf(response, sizeof(response), "MSG FROM Server IS %s has joined %s.\r\n", client->displayName, client->channel);
                send (client->fd, response, strlen(response), 0);
                sleep(1);
                //log_message("SENT", client->fd, response);
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
                log_message("RECV", client->fd, client->buffer, "MSG");
                broadcast_message(get_or_create_channel(client->channel), client->buffer, client);
                sleep(1);
            } else {
                send(client->fd, "ERR FROM Server IS Invalid MSG format\r\n", 38, 0);
                sleep(1);
            }
        } else if (strcmp(client->buffer, "BYE\r\n") == 0) {
            log_message("RECV", client->fd, client->buffer, "BYE");
            send(client->fd, "BYE\r\n", 5, 0);
            leave_channel(client);
            close(client->fd);
            client->fd = -1;
            client->state = END_STATE;
        }  else if(strcmp(command, "ERR") == 0){
            if ((sscanf(client->buffer, "ERR FROM %s IS %[^\r\n]", client->displayName, messageContent) == 2)){
                log_message("RECV", client->fd, client->buffer, "ERR");
                client->state = ERROR_STATE;
            }else{
                log_message("RECV", client->fd, client->buffer, "ERR");
                client->state = ERROR_STATE;
            }

        }
        else {
            log_message("RECV", client->fd, client->buffer, "UNKNOWN");
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


void log_message(const char* prefix, int fd, const char* message, const char* command) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    char formatted_content[1024] = {0}; // Buffer to hold formatted message content

    if (getpeername(fd, (struct sockaddr *)&addr, &addr_len) == -1) {
        perror("getpeername failed");
        return;
    }

    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, clientIP, INET_ADDRSTRLEN);
    int clientPort = ntohs(addr.sin_port);

    // Parse message content based on the command
    if (strcmp(command, "AUTH") == 0) {
        char username[100], displayName[100], secret[100];
        if (sscanf(message, "AUTH %s AS %s USING %s", username, displayName, secret) == 3) {
            snprintf(formatted_content, sizeof(formatted_content), "Username=%s DisplayName=%s Secret=%s", username, displayName, secret);
        }
    } else if (strcmp(command, "JOIN") == 0) {
        char channelID[100], displayName[100];
        if (sscanf(message, "JOIN %s AS %s", channelID, displayName) == 2) {
            snprintf(formatted_content, sizeof(formatted_content), "ChannelID=%s DisplayName=%s", channelID, displayName);
        }
    } else if (strcmp(command, "MSG") == 0) {
        char displayName[100], msgContent[512];
        if (sscanf(message, "MSG FROM %s IS %[^\r\n]", displayName, msgContent) == 2) {
            snprintf(formatted_content, sizeof(formatted_content), "DisplayName=%s MessageContent=%s", displayName, msgContent);
        }
    } else if (strcmp(command, "REPLY") == 0) {
        char responseType[10], msgContent[512];
        if (sscanf(message, "REPLY %9s IS %[^\r\n]", responseType, msgContent) == 2) {
            // Check for positive or negative reply and format accordingly
            if (strcmp(responseType, "OK") == 0) {
                snprintf(formatted_content, sizeof(formatted_content), "Reply=OK MessageContent=%s", msgContent);
            } else {
                snprintf(formatted_content, sizeof(formatted_content), "Reply=NOK MessageContent=%s", msgContent);
            }
        }
    } else if (strcmp(command, "ERR") == 0) {
        char msgContent[512];
        if (sscanf(message, "ERR FROM %*s IS %[^\r\n]", msgContent) == 1) {
            snprintf(formatted_content, sizeof(formatted_content), "ErrorDetail=%s", msgContent);
        }
    }

    // Output the formatted log entry
    printf("%s %s:%d | %s %s\n", prefix, clientIP, clientPort, command, formatted_content);
}
