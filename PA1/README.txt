Julia Sanford
Network Systems
Programming Assignment 1
UDP Socket Programming


Files
- client/makefile: a makefile to compile the client code
- server/makefile: a makefile to compile the server code
- client/uftp_client.c: a C file that connects to a server and sends commands to the server
- server/uftp_server.c: a C file that connects to a client and receives and executes commands to the client


What this program does
	This program creates a UDP connection with a client to a server's IP address over a port. The client provides a menu with options to manipulate files. The selected command is sent to the server. The client and server communicate in order to execute the commands.


How to run this program
1. Identify the IP of the server and a common port number to use.
2. In the client directory, run 'make' to compile the client code.
3. In the server directory, run 'make' to compile the server code.
4. In the server directory, run './server <port number>'.
5. In the client directory, run './client <server IP address> <port number>'.
