CC=gcc
CFLAGS=-I. -Wall -Wextra -pedantic -std=c99 -g

TARGET=ipk24server

SOURCES=server.c validation.c cli.c


OBJECTS=$(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJECTS)
