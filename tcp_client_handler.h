#ifndef TCP_CLIENT_HANDLER_H
#define TCP_CLIENT_HANDLER_H

#include "client.h"  // Include the client structure definition

void handle_accept_state(Client *client);
void handle_auth_state(Client *client);
void handle_open_state(Client *client);
void handle_error_state(Client *client);
void handle_end_state(Client *client);


#endif
