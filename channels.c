#include "channels.h"
#include <stdio.h>
#include <string.h>


// Global array of channels
static Channel channels[MAX_CHANNELS];

// Find a channel by its ID in the global array
Channel* find_channel_by_id(const char* channelID) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (strcmp(channels[i].channelName, channelID) == 0) {
            return &channels[i];
        }
    }
    return NULL;
}
// Get or create a channel by its name
Channel* get_or_create_channel(const char* channelName) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (strcmp(channels[i].channelName, channelName) == 0) {
            return &channels[i];
        }
    }

    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].channelName[0] == '\0') {
            strncpy(channels[i].channelName, channelName, MAX_CHANNEL_ID_LENGTH - 1);
            channels[i].clientCount = 0;
            return &channels[i];
        }
    }

    return NULL;
}
// Leave a channel by removing the client from the channel
void leave_channel(Client *client) {
    if (client->channel[0] == '\0') {
        return;
    }

    Channel* channel = find_channel_by_id(client->channel);
    if (!channel) {
        return;
    }

    for (int i = 0; i < channel->clientCount; i++) {
        if (channel->clients[i] == client) {
            memmove(&channel->clients[i], &channel->clients[i + 1], (channel->clientCount - i - 1) * sizeof(Client*));
            channel->clientCount--;
            memset(client->channel, 0, MAX_CHANNEL_ID_LENGTH);
            break;
        }
    }
}

// Join a channel by adding the client to the channel
int join_channel(Client *client, const char* channelName) {
    leave_channel(client); // Ensure leaving previous channel

    Channel* channel = get_or_create_channel(channelName);
    if (!channel) {
        return -1;
    }

    if (channel->clientCount < MAX_CLIENTS) {
        channel->clients[channel->clientCount++] = client;
        strncpy(client->channel, channelName, MAX_CHANNEL_ID_LENGTH - 1);
        return 0;
    }

    return -1;
}

// Broadcast a message to all clients in the channel
void broadcast_message(Channel *channel, const char *message, Client *sender) {
    for (int i = 0; i < channel->clientCount; i++) {
        Client *client = channel->clients[i];
        if (client && client != sender && client->channel[0] != '\0' && client->fd > 0) {
            if (send(client->fd, message, strlen(message), 0) == -1) {
                perror("send failed");
                close(client->fd);
                client->fd = -1;

            }
        }
    }
}
