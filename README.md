# **IPK Project 2**

## Description

This project develops a server application, which is capable to communicate with remote clients using `IPK24-CHAT` protocol. The server is capable of managing multiple client connections, supporting two transport protocols: TCP and UDP. 

The server is built to handle key functionalities including client authentication, channel management, and message broadcasting among connected clients. Implemented in C and uses socket API for network communications.  Behvaior of the server is described in the FSM diagram below.


![Alt text](https://git.fit.vutbr.cz/NESFIT/IPK-Projects-2024/media/branch/master/Project%202/iota/diagrams/protocol_fsm_server.svg)


## IPK24-CHAT Protocol

### Protocol Features:
The IPK24-CHAT protocol is designed to implement client-server communication for a chat application. The protocol defines a set of message types and operations that clients and servers can use to interact with each other. The protocol features include:
- **Transport Layer Flexibility**: Works on both TCP and UDP transport protocols.
- **Dual Protocol Support**: Supports both TCP and UDP transport protocols for client-server communication.
- **State Management**: Implements a finite state machine (FSM) to manage client-server interactions.
- **Error Handling**: Provides detailed error messages for invalid client requests or server operations.
- **Message Confirmation**: In UDP mode, requires message confirmations with `CONFIRM` message type.
- **Message Broadcasting**: Supports message broadcasting to multiple clients in the same channel.

#### Message Types

Below are the different types of messages used in the IPK24-CHAT protocol:

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
./ipk24server -l <IP address> -p <port> -d <timeout> -r <retries> -h 
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


### Main function



### TCP client
The TCP client first creates a socket with the `socket()` function. The client then finds the server with the `gethostbyname()` function and creates a structure `server` with the server's IP address and port number. The client then connects to the server with the `connect()` function.
Then the client operates in a loop. First, it gets input from the user with the `fgets()` function. The input is sent to the server with the `send()` function. The client then waits for a response from the server with the `recv()` function. The response is printed to the user. The loop have the same states as FSM diagram above. The client can be terminated by pressing `Ctrl+C`, which closes the socket, thus terminating the connection, and exits the program.

For handling responses from the server in the TCP mode was implemented `handle_response()` function. The function parse the response from the server and prints the appropriate message to the user. Also this function return integer value, which is used to determine if the client should continue in the loop or go to other state.

To handle input from a network socket and standard input (stdin) without blocking the program's execution was used `POLL`.
Setup of `POLL` involves:
1. Creating a `struct pollfd` array with two elements, one for the network socket and one for stdin.
2. Setting the `fd` field of the network socket element to the socket file descriptor.
3. Setting the `events` field of the network socket element to `POLLIN`.
4. Setting the `fd` field of the stdin element to `0`.
5. Setting the `events` field of the stdin element to `POLLIN`.

`POLL` is called inside of the loop and waiting for the response from the server or input from the user. If the response is received, the `handle_response()` function is called. If the user inputs something, the input is sent to the server.

To find the server was used `gethosbyname()` function. The function returns a pointer to a `struct hostent` structure, which contains information about the server. The `struct hostent` structure contains the server's IP address and port number.

### UDP client
The UDP client use the same structure as the TCP client. The client first creates a socket with the `socket()` function. The client then finds the server with the `gethostbyname()` function and creates a structure `server` with the server's IP address and port number.
UDP client use FSM logic to switch between states.

For checking stdin messages from user were implemented functions like `Check_username()` and others.

For avoiding packet duplication was used MessageID. MessageID is converting in network order using `htons()` function: `uint16_t networkOrderMessageID = htons(messageID)`.


For packet loss was implemented retransmission of the packet. The client sends the packet and waits for the response. If the response is not received within the timeout, the client resends the packet. The client resends the packet up to the maximum number of retries. If the response is not received after the maximum number of retries, the client return `ERROR_STATE`.
To avoid packet loss was implemented function `wait_confirm()`, which waits for the response from the server. The function uses `POLL` to wait for the response. If the response is received, the function returns  0. If the response is not received within the timeout, the function returns 1.


### Hostname Resolution
The client supports connecting to the server using a hostname, thanks to the `gethostbyname()` function for IPv4. This function resolves the server's hostname to its IP address, enabling the client to establish connections without requiring the server's IP address directly.

### Graceful Shutdown
The client is designed to terminate gracefully in response to an interrupt signal (e.g., Ctrl+C). Upon receiving the signal, the client closes the socket, ensuring a proper shutdown of the connection with the server. For the implementation of signal handling was used `signal()` function.
### Error handling
Robust error and signal handling mechanisms are integral to the client's design. The client can handle various errors, such as invalid command-line arguments, connection failures, and data transmission errors, providing appropriate feedback to the user.



## Testing
For testing the ipk24-chat client, two methods were used, tailored to TCP and UDP protocols respectively. Testing was made manually by running the client and server on separate terminals and observing the interaction between them.
The client was tested on macos and linux operating systems.
### TCP Testing
TCP was testing using netcat utility. The server was started with the following command:
```bash
nc -4 -c -l -v 127.0.0.1 4567
```
The client was started with the following command:
```bash
./ipk24chat-client -t tcp -s 127.0.0.1 -p 4567
```
#### Examples of the client input and output are shown below:
```bash
/auth username secret Display_Name
Success: Auth success.
Display_Name: Hello
Server: Hello
Display_Name: bye
```
```bash
/auth Tom secret Tomik
Success: Auth success.
Tomik: Hello
Server: Hello
/join channel22
Success: Join success.
Server: Tomik has joined channel22.
Tomik: Bye
```
#### Examples from Wireshark:

Simple TCP conversation:
```bash
AUTH tom AS Tomik USING secret
                              REPLY OK IS Auth success.
                              MSG FROM Server IS Tomik joined default.
                              MSG FROM Server IS Tomik Hello
MSG FROM Tomik IS Hi
MSG FROM Tomik IS Bye
BYE
```
Bye message from client:
```bash
AUTH tom AS Tomik USING secret
                              REPLY OK IS Auth success.
BYE
```

### UDP Testing
For testing UDP was used servrer from [2]. The server was started with the following command:
```bash
 python3 ipk_server.py
```
The client was started with the following command:
```bash
./ipk24chat-client -t udp -s 127.0.0.1 -p 4567
```

#### Examples of the client input and output are shown below:
```bash
/auth a b Tim
Success: Hi, Tim, this is a successful REPLY message to your AUTH message id=0. You wanted to authenticate under the username a
Tim: Hi
Server: Hi, Tim! This is a reply MSG to your MSG id=256 content='Hi...' :)
/join channel2
Success: Hi, Tim, this is a successful REPLY message to your JOIN message id=512. You wanted to join the channel channel2
```

```bash
/auth user1 secret Kaja
Success: Hi, Kaja, this is a successful REPLY message to your AUTH message id=0. You wanted to authenticate under the username user1
Kaja: Bye
Server: Hi, Kaja! This is a reply MSG to your MSG id=256 content='Bye...' :) 
```

#### Compare MessageID and screenshot from Wireshark:
```bash
/auth user secret Max
Success: Hi, Max, this is a successful REPLY message to your AUTH message id=0. You wanted to authenticate under the username user
hi
Max: hi
Server: Hi, Max! This is a reply MSG to your MSG id=256 content='hi...' :)
```
![Example Image](messageID.png)
MessageIDs are correct.

#### Sending Bye from client and screenshot from Wireshark:
```bash
/auth user secret Kaja
Success: Hi, Kaja, this is a successful REPLY message to your AUTH message id=0. You wanted to authenticate under the username user
Ahoj
Kaja: Ahoj
Server: Hi, Kaja! This is a reply MSG to your MSG id=256 content='Ahoj...' :)
Ctrl + c
```
![Example Image](Bye.png)
Bye was send from client and server received it.

## References

[1]: DOLEJŠKA, Daniel. PK-Projects-2024/Project 1 [online]. 2024 [cit. 2024-04-01]. Dostupné z: https://git.fit.vutbr.cz/NESFIT/IPK-Projects-2024/src/branch/master/Project%201

[2]: Ipk_server.py [online]. 2024 [cit. 2024-04-01]. Available at: https://github.com/okurka12/ipk_proj1_livestream/blob/main/ipk_server.py

[3]: Beej's Guide to Network Programming: Using Internet Sockets [online]. 2023 [cit. 2024-04-01]. Available at: https://beej.us/guide/bgnet/html/. 