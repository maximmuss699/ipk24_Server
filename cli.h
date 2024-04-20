#ifndef CLI_H
#define CLI_H

#include <stdint.h>
#include <getopt.h>
// Define necessary constants
#define INET_ADDRSTRLEN 16

// Structure for global configuration
struct {
    char server_ip[INET_ADDRSTRLEN];
    uint16_t server_port;
    uint16_t udp_timeout;
    uint8_t udp_retries;
} config;

// Function declarations
void print_usage(void);
void parse_arguments(int argc, char* argv[]);

#endif // CLI_H
