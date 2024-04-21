CC=gcc
CFLAGS=-I. -Wall -Wextra -pedantic -std=c99 -g
LDFLAGS=-pthread

TARGET=ipk24server
SOURCES=server.c validation.c cli.c channels.c tcp_client_handler.c
OBJECTS=$(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

# Dependencies to ensure objects are rebuilt if headers change
server.o: server.c validation.h cli.h channels.h
validation.o: validation.c validation.h
cli.o: cli.c cli.h
channels.o: channels.c channels.h
tcp_client_handler.o: tcp_client_handler.c tcp_client_handler.h

.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJECTS)
