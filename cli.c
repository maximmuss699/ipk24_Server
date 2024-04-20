#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For getopt

void print_usage() {
    printf("Usage: server [options]\n");
    printf("Options:\n");
    printf("  -l <IP address>          Server listening IP address for welcome sockets (default: 0.0.0.0)\n");
    printf("  -p <port>                Server listening port for welcome sockets (default: 4567)\n");
    printf("  -d <timeout>             UDP confirmation timeout in milliseconds (default: 250)\n");
    printf("  -r <retries>             Maximum number of UDP retransmissions (default: 3)\n");
    printf("  -h                       Prints this help output and exits\n");
}

void parse_arguments(int argc, char* argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "l:p:d:r:h")) != -1) {
        switch (opt) {
            case 'l':
                strncpy(config.server_ip, optarg, INET_ADDRSTRLEN);
                break;
            case 'p':
                config.server_port = (uint16_t)atoi(optarg);
                break;
            case 'd':
                config.udp_timeout = (uint16_t)atoi(optarg);
                break;
            case 'r':
                config.udp_retries = (uint8_t)atoi(optarg);
                break;
            case 'h':
                print_usage();
                exit(EXIT_SUCCESS);
            default:
                print_usage();
                exit(EXIT_FAILURE);
        }
    }
}
