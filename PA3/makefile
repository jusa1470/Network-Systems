CC=gcc
CFLAGS = -w -g -Wall -pthread
HASH = -lcrypto -lssl

all: webproxy.c
	$(CC) $(CFLAGS) -o webproxy webproxy.c $(HASH)

clean:
	$(RM) webproxy