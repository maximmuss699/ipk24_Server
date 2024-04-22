// client.h
#ifndef CLIENT_H
#define CLIENT_H

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

#define BUFFER_SIZE 1024
#define MAX_USERNAME_LENGTH 20
#define MAX_SECRET_LENGTH 128
#define MAX_DISPLAY_NAME_LENGTH 20
#define MAX_CHANNEL_ID_LENGTH 20
#define POLL_TIMEOUT 2000000 // Timeout for poll in milliseconds
#define DEFAULT_CHANNEL "default"
#define MAX_MSG_LEN 2048

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
    struct sockaddr_in addr;
} Client;

void log_message(const char* prefix, int fd, const char* message, const char* command);
void handle_accept_state(Client *client);
void handle_auth_state(Client *client);
void handle_open_state(Client *client);
void handle_error_state(Client *client);
void handle_end_state(Client *client);


#endif // CLIENT_H
