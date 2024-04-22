#ifndef CHANNEL_H
#define CHANNEL_H

#include "client.h"  // Assuming you have a client structure defined elsewhere

#define MAX_CHANNELS 100
#define MAX_CLIENTS 100
#define MAX_CHANNEL_ID_LENGTH 20


typedef struct {
    char channelName[MAX_CHANNEL_ID_LENGTH];
    Client *clients[MAX_CLIENTS];  // Pointers to clients in this channel
    int clientCount;
} Channel;

Channel* find_channel_by_id(const char* channelID);
Channel* get_or_create_channel(const char* channelName);
void leave_channel(Client *client);
int join_channel(Client *client, const char* channelName);
void broadcast_message(Channel *channel, const char *message, Client *sender);

#endif // CHANNEL_H
