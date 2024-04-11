CC=gcc
CFLAGS=-I. -Wall -Wextra -pedantic -std=c99 -g

all: ipk24server

ipk24server: server.c
	$(CC) -o ipk24server  server.c $(CFLAGS)

.PHONY: clean
clean:
	rm -f ipk24server
