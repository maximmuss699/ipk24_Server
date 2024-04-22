# **IPK Project 2**

## Description

This project develops a server application, which is capable to communicate with remote clients using `IPK24-CHAT` protocol [Project1]. The server is capable of managing multiple client connections, supporting two transport protocols: TCP [RFC9293] and UDP [RFC768]. 

The server is built to handle key functionalities including client authentication, channel management, and message broadcasting among connected clients. Implemented in C and uses socket API for network communications.  Behvaior of the server is described in the FSM diagram below [Project2].


![Alt text](https://git.fit.vutbr.cz/NESFIT/IPK-Projects-2024/media/branch/master/Project%202/iota/diagrams/protocol_fsm_server.svg)


## IPK24-CHAT Protocol

### Protocol Features:
The IPK24-CHAT protocol is made to implement client-server communication for a chat application. The protocol defines a set of message types and operations that clients and servers can use to interact with each other. The protocol features include:
- **Transport Layer Flexibility**: Works on both TCP and UDP transport protocols.
- **Dual Protocol Support**: Supports both TCP and UDP transport protocols for client-server communication.
- **State Management**: Implements a finite state machine (FSM) to manage client-server interactions.
- **Error Handling**: Provides detailed error messages for invalid client requests or server operations.
- **Message Confirmation**: In UDP mode, requires message confirmations with `CONFIRM` message type.
- **Message Broadcasting**: Supports message broadcasting to multiple clients in the same channel.

#### Message Types

Below are the different types of messages used in the IPK24-CHAT protocol[Project1]:

| Message Type |  Description |
|--------------|-------------|
| **AUTH**     | Used for client authentication by requiring a username, display name, and password. |
| **JOIN**     | Allows a client to request joining a specific chat channel. |
| **MSG**      | Used for sending messages to other clients within the same channel. |
| **ERR**      | Indicates an error in the client's request or an issue during server operation. |
| **REPLY**    | Provides a positive or negative response to `AUTH`, `JOIN`, and other operations. |
| **BYE**      | Used by a client to indicate the intention to disconnect from the server. |


### Protocol Operations:
The IPK24-CHAT protocol defines the following operations for client-server communication:
- **Connection**: Clients connect to the server using TCP or UDP transport protocols.
- **Authentication**: Clients must authenticate by sending an `AUTH` message immediately after establishing the connection.
- **Channel Management**: After authentication, clients can join channels using the `JOIN` message and can between switch channels.
- **Messaging**: Authenticated and joined clients can send messages using the `MSG` type, which are then broadcasted to all members of the channel.
- **Session Termination**: Clients or server can end the session by sending a `BYE` message, followed by closing the connection.



## Usage
```bash
./ipk24chat-server -l <IP address> -p <port> -d <timeout> -r <retries> -h 
```


```
Options:
-l <IP address>    Server listening IP address for welcome sockets 
-p <port>          Server listening port for welcome sockets 
-d <timeout>       UDP confirmation timeout in milliseconds .
-r <retries>       Maximum number of UDP retransmissions.
-h                 Display this help message and exit.
```
## Implementation
This section describes how the server manage multiple client connections using TCP and UDP protocols.

### Main function
The main function first parses the command line arguments with the `getopt()` function. Check if the arguments are valid and then initializes the server with the provided IP address and port number. The server then enters the main loop, where uses `select()` to handle incoming TCP connection requests, TCP client data, and UDP packets.
. It uses `poll` and threads to manage parallel connections efficiently. 


### TCP Client Management
When a TCP client connects, the server sets up a dedicated socket for this specific interaction. Then it uses a separate thread to handle the client.
As data comes in from the client, the server processes it based on the current state of the client.
For handling TCP clients was made `Client` structure, which contains the client's socket, state, buffer, channel and other information about the client. 

The main FSM logic for TCP clients was implemented in the `tcp_client_handler.c` file. Here you also can find `log_message()`function for logging server input/output. 

For channel management it uses functions from `channels.c` file. When the Client firstly authorizes, it joins the default channel and then can switch between channels.
After authorization the Server added the client to the default channel using `join_channel()` function and sends a broadcast message to the channel using `broadcast_message()` function.
### UDP Client Management
UDP clients are managed differently than TCP clients. The server uses a single UDP socket to communicate with all UDP clients and uses the same `Client` structure to store information about the UDP client, including the client's address, port, and other information.

For managing UDP clients were implemented logic for managing `Client sessions`. Each UDP client is identified by their unique network address, which includes their IP address and port number. This helps the server recognize who is sending the data and ensures that replies are sent to the correct client.

When the server receives data from a new client, it sets up a new `session` for that client using their network address. This helps the server manage different clients separately. Each session can be in different states.

Depending on the session's current state, different types of messages are expected and processed differently. For example, only in the OPEN_STATE can a client send chat messages to others. The server handles each message according to its type and the session's state, ensuring confrims are sent back.

It also uses the same functions for channel management as TCP clients and the same FSM logic for managing the client's state.

### Channel Management
All channel management functions are implemented in the `channels.c` file. There are functions like `join_channel()`, `leave_channel()`, `get_or_create_channel()` and `broadcast_message()`. 

`Broadcast_message()` function uses `for` loop to send the message to all clients in the channel. It sends the message to all clients except the sender and uses the `send()` function to send the message to the every client's socket in this channel.


### Client Structure

The `Client` structure is made to store information about each network client, including both TCP and UDP clients:

| Field Name    | Type             | Description                                                    | Limit                    |
|---------------|------------------|----------------------------------------------------------------|--------------------------|
| `fd`          | `int`            | File descriptor for the client's socket connection.            | N/A                      |
| `state`       | `State`          | Current state of the client, managing session states.          | Enumerated Type          |
| `buffer`      | `char[]`         | Temporary storage for data received from or sent to client.    | `BUFFER_SIZE`            |
| `username`    | `char[]`         | Username of the client.                                        | `MAX_USERNAME_LENGTH`    |
| `channel`     | `char[]`         | Name of the channel to which the client is connected.          | `MAX_CHANNEL_ID_LENGTH`  |
| `secret`      | `char[]`         | Client's authentication secret.                                | `MAX_SECRET_LENGTH`      |
| `displayName` | `char[]`         | Display name of the client.                                    | `MAX_DISPLAY_NAME_LENGTH`|
| `addr`        | `struct sockaddr_in` | Client’s network address, including IP and port.               | N/A                      |
| `addr_len`    | `socklen_t`      | Length of the client’s address.                                | N/A                      |
| `active`      | `int`            | Indicates whether the client is active (1) or not (0)(for UDP) | N/A                      |




### Graceful connection termination
The server can handle client disconnections gracefully. When a client sends a `BYE` message, the server removes the client from the channel and closes the client's socket. The server then frees the memory allocated for the client structure and exits the client thread.

### Error handling
When the server receives the `ERR` message from the client it try to gracefully close the connection and free the memory allocated for the client structure. Server and Client sends `BYE` message to each other and then close the connection.

## Testing
The server was tested using both TCP and UDP protocols. It was tested with multiple clients connecting simultaneously and sending messages to each other. Tested on macOS 14 and Ubuntu 20.
### TCP clients testing
TCP clients were tested using the server from [ipk24chat-client] . The server was started with the following command:
```bash
./ipk24chat-server -l 127.0.0.1 -p 4567
```
The client was started with the following command:
```bash
./ipk24chat-client -t tcp -s 127.0.0.1 -p 4567
```
#### Examples of client and server outputs are shown below:
`Simple conversation of one TCP client`:

Client authenticates, joins the default channel, sends a message, switches to another channel, sends a message, and then disconnects.

Client output:
```bash
/auth user secret TIM
Success: Auth success.
Server: TIM has joined default.
TIM: Hello
/join channel2
Success: Join success.
Server: TIM has joined channel2.
TIM: Bye
```
Server output:
```bash
RECV 127.0.0.1:51109 | AUTH Username=user DisplayName=TIM Secret=secret
SENT 127.0.0.1:51109 | REPLY Reply=OK MessageContent=Auth success.
SENT 127.0.0.1:51109 | MSG DisplayName=Server MessageContent=TIM has joined default.
RECV 127.0.0.1:51109 | MSG DisplayName=TIM MessageContent=Hello
RECV 127.0.0.1:51109 | JOIN ChannelID=channel2 DisplayName=TIM
SENT 127.0.0.1:51109 | REPLY Reply=OK MessageContent=Join success.
RECV 127.0.0.1:51109 | MSG DisplayName=TIM MessageContent=Bye
RECV 127.0.0.1:51109 | BYE 

```

`Simple conversation of two TCP clients`:

Two clients authenticate, join the default channel, send messages, switch to another channel, send messages, and then disconnect.

Clients output:    
```bash
/auth user1 secret1 MAX            /auth user2 secret TOM
Success: Auth success.             Success: Auth success.
Server: MAX has joined default.    Server: TOM has joined default.
Server: TOM has joined default.  
MAX: HI                            MAX: HI
TOM: HELLO                         TOM: HELLO
/join channel2                     /join channel2
Success: Join success.             Success: Join success.
Server: MAX has joined channel2.   Server: TOM has joined channel2.
Server: TOM has joined channel2.   
MAX: BYE                           TOM: BYE
```
Server output:
```bash
RECV 127.0.0.1:51158 | AUTH Username=user1 DisplayName=MAX Secret=secret1
SENT 127.0.0.1:51158 | REPLY Reply=OK MessageContent=Auth success.
SENT 127.0.0.1:51158 | MSG DisplayName=Server MessageContent=MAX has joined default.
RECV 127.0.0.1:51161 | AUTH Username=user1 DisplayName=MAX Secret=secret1
SENT 127.0.0.1:51161 | REPLY Reply=OK MessageContent=Auth success.
SENT 127.0.0.1:51161 | MSG DisplayName=Server MessageContent=MAX has joined default.
RECV 127.0.0.1:51158 | MSG DisplayName=MAX MessageContent=HI
RECV 127.0.0.1:51161 | MSG DisplayName=TOM MessageContent=HELLO
RECV 127.0.0.1:51158 | JOIN ChannelID=channel2 DisplayName=MAX
SENT 127.0.0.1:51158 | REPLY Reply=OK MessageContent=Join success.
RECV 127.0.0.1:51161 | JOIN ChannelID=channel2 DisplayName=TOM
SENT 127.0.0.1:51161 | REPLY Reply=OK MessageContent=Join success.
RECV 127.0.0.1:51158 | MSG DisplayName=MAX MessageContent=BYE
RECV 127.0.0.1:51161 | MSG DisplayName=TOM MessageContent=BYE
RECV 127.0.0.1:51158 | BYE 
RECV 127.0.0.1:51161 | BYE 


```

`Conversation where clients are in different channels and they dont see each other messages`:

Two clients authenticate, join different channels, send messages, and then disconnect.

Client output:
```bash
/auth user1 secret1 ALEX               /auth user2 secret2 NIK
Success: Auth success.                 Success: Auth success.
Server: ALEX has joined default.       Server: NIK has joined default.
Server: NIK has joined default.
/join channel
Success: Join success.
Server: ALEX has joined channel.       Server: ALEX has left default.
ALEX: HI
ALEX: HELLO
ALEX: BYE

```
Server output:
```bash
RECV 127.0.0.1:51214 | AUTH Username=user1 DisplayName=ALEX Secret=secret1
SENT 127.0.0.1:51214 | REPLY Reply=OK MessageContent=Auth success.
SENT 127.0.0.1:51214 | MSG DisplayName=Server MessageContent=ALEX has joined default.
RECV 127.0.0.1:51215 | AUTH Username=user1 DisplayName=ALEX Secret=secret1
SENT 127.0.0.1:51215 | REPLY Reply=OK MessageContent=Auth success.
SENT 127.0.0.1:51215 | MSG DisplayName=Server MessageContent=ALEX has joined default.
RECV 127.0.0.1:51214 | JOIN ChannelID=channel DisplayName=ALEX
SENT 127.0.0.1:51214 | REPLY Reply=OK MessageContent=Join success.
RECV 127.0.0.1:51214 | MSG DisplayName=ALEX MessageContent=HI
RECV 127.0.0.1:51214 | MSG DisplayName=ALEX MessageContent=HELLO
RECV 127.0.0.1:51214 | MSG DisplayName=ALEX MessageContent=BYE
RECV 127.0.0.1:51214 | BYE 
RECV 127.0.0.1:51215 | BYE 


```

#### Examples from Wireshark:

`Simple conversation of two TCP clients`:

The same conversation as above but in Wireshark.
```bash
AUTH user1 AS MAX USING secret1.
                              REPLY OK IS Auth success.
                              MSG FROM Server IS MAX joined default.
AUTH user2 AS TOM USING secret.
                              REPLY OK IS Auth success.
                              MSG FROM Server IS TOM joined default.
                              MSG FROM Server IS TOM joined default. (Broadcast message to MAX)
MSG FROM MAX IS HI.
                              MSG FROM MAX IS HI (Broadcast message to TOM)
MSG FROM TOM IS HELLO.
                              MSG FROM TOM IS HELLO (Broadcast message to MAX)
JOIN channel2 AS MAX.
                              MSG FROM Server IS MAX has left default.                
                              REPLY OK IS Join success.
                              MSG FROM Server IS MAX joined channel2. 
JOIN channel2 AS TOM.
                              MSG FROM Server IS TOM joined channel2. (Broadcast message to MAX)
                              REPLY OK IS Join success.
                              MSG FROM Server IS TOM joined channel2.
BYE
BYE
```
`Client uses /rename command during the conversation`:

Client started the conversation with DisplayName=SAM, then renamed himself into DisplayName=SAM22 and continued the conversation.
```bash
AUTH user AS SAM USING secret.
                                REPLY OK IS Auth success.
                                MSG FROM Server IS SAM joined default.
(SAM changed his DisplayName to SAM22)
JOIN channel2.
                                REPLY OK IS Join success.
                                MSG FROM Server IS SAM22 joined channel2.
BYE
```

### UDP Testing
UDP clients were tested using the server from [ipk24chat-client]. Handling UDP clients wasnt fully implemented. The server was started with the following command:
```bash
 ./ipk24chat-server -l 127.0.0.1 -p 4567
```
The client was started with the following command:
```bash
./ipk24chat-client -t udp -s 127.0.0.1 -p 4567
```

Handling of UDP clients not fully implemented. Server not fully supports UDP clients. It can handle clients in `ACCEPT`, `AUTH` and partially in`OPEN` states.

#### Examples of client and server outputs are shown below:

`Simple conversation of one UDP client`:

Client output:
```bash
/auth user secret TOM
Success: Auth success.
Server: TOM has joined default.
TOM: Hi
TOM: Bye
```


## References

[Project1] Dolejška, D. _Client for a chat server using IPK24-CHAT protocol_ [online]. February 2024. [cited 2024-04-22]. Available at: https://git.fit.vutbr.cz/NESFIT/IPK-Projects-2024/src/branch/master/Project%201

[ipk24chat-client] Samusevich, M. _IPK Project 1_ [online]. April 2024. [cited 2024-04-22]. Available at: https://git.fit.vutbr.cz/xsamus00/IPK24

[Project2] Dolejška, D. _Chat server using IPK24-CHAT protocol_ [online]. February 2024. [cited 2024-04-22]. Available at: https://git.fit.vutbr.cz/NESFIT/IPK-Projects-2024/src/branch/master/Project%202/iota#ipk-project-2-chat-server-using-ipk24-chat-protocol

[RFC9293] Eddy, W. _Transmission Control Protocol (TCP)_ [online]. August 2022. [cited 2024-04-22]. DOI: 10.17487/RFC9293. Available at: https://datatracker.ietf.org/doc/html/rfc9293

[RFC768] Postel, J. _User Datagram Protocol_ [online]. March 1997. [cited 2024-04-22]. DOI: 10.17487/RFC0768. Available at: https://datatracker.ietf.org/doc/html/rfc768

