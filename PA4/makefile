CC=gcc
CFLAGS = -w -g -Wall -pthread
HASH = -lcrypto -lssl

all: client server

client:
	$(CC) $(CFLAGS) -o dfc client.c $(HASH)

server:
	$(CC) $(CFLAGS) -o dfs server.c $(HASH)

clean:
	$(RM) dfc
	$(RM) dfs
	$(RM) -rf ./DFS1/*
	$(RM) -rf ./DFS2/*
	$(RM) -rf ./DFS3/*
	$(RM) -rf ./DFS4/*
	$(RM) -rf ./files/*