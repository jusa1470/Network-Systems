#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/sendfile.h>

#define BUFSIZE 1024

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    /* check command line arguments */
    if (argc != 3) {
    	fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
    	exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
    	error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
    	fprintf(stderr,"ERROR, no such host as %s\n", hostname);
    	exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);


    /* repeat menu until exit command is called */
    while (1) {
    	// print menu
    	printf("-----MENU-----\n");
    	printf("1. get [file_name]\n");
    	printf("2. put [file_name]\n");
    	printf("3. delete [file_name]\n");
    	printf("4. ls\n");
    	printf("5. exit\n");

	    /* get a message from the user */
	    bzero(buf, BUFSIZE);
	    printf("Enter command: ");
	    fgets(buf, BUFSIZE, stdin);

	    /* send the message to the server */
	    serverlen = sizeof(serveraddr);
	    n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
	    if (n < 0) 
	     	error("ERROR in sendto");


	    /*-----------Handle Commands-----------*/
	    /* get command */
	    if (strstr(buf, "get") != NULL) { // if command is get
	      // parse the filename out
	      char buf2[BUFSIZE];
	      strcpy(buf2, buf);
	      char *token = strtok(buf2, " ");
	      token = strtok(NULL, " ");

	      if (token != NULL) { // if filename is in the command
	        // strip newline and create file pointer
	        token[strlen(token)-1] = '\0';
	        FILE *file;

	        // handshake for whether the file exists or not
	        bzero(buf, BUFSIZE);
	        recvfrom(sockfd, buf, BUFSIZE, 0, &serveraddr, &serverlen);

	        if (strcmp(buf, "File exists.\n") == 0) { // if the file exists, then receive data
	          if (file = fopen(token, "wb")) { // if the file opens/creates succesfully
	            // begin receiving file contents
	            int nbytes, bytes_written;
	            do {
	              bzero(buf, BUFSIZE);
	              nbytes = recvfrom(sockfd, buf, BUFSIZE, 0, &serveraddr, &serverlen);
	              if (buf[nbytes-1] == EOF) { // if last character is EOF
	                nbytes = nbytes - 1; // don't write the EOF char
	              }
	              bytes_written = fwrite(buf, 1, nbytes, file);
	            } while (bytes_written == BUFSIZE); // while there is still data to be written

	            fclose(file);
	          }
	          else { // if the file does not open/create successfully
	            printf("Failed to create file on the client side.\n");
	          }
	        }
	      }
	    }

	    /* put command */
	    else if (strstr(buf, "put") != NULL) {
				// parse filename from input
				char buf2[BUFSIZE];
	      strcpy(buf2, buf);
	      char *token = strtok(buf2, " ");
	      token = strtok(NULL, " ");

				if (token != NULL)  { // if filename is in the command
					// strip newline and create file pointer
					token[strlen(token)-1] = '\0';
					FILE *file;

					if (file = fopen(token, "rb")) { // if file exists
						// handshake to make sure file exists
						bzero(buf, BUFSIZE);
						strcpy(buf, "File exists.\n");
						sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);

						int bytes_read;
						do { // begin sending data

				    	bzero(buf, BUFSIZE);
							bytes_read = fread(buf, 1, BUFSIZE, file);
							printf("buffer:\n%s\n", buf);
				    	sendto(sockfd, buf, bytes_read, 0, &serveraddr, serverlen);
				    	
				    } while (bytes_read == BUFSIZE);

				  	fclose(file);
					}
					else { // if file does not exist
						bzero(buf, BUFSIZE);
						strcpy(buf, "File does not exist.\n");
						sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
					}
				}
			}

	    /* exit command */
	    else if (strcmp(buf, "exit\n") == 0) {
	    	break;
	    }


	    
	    /* print the server's reply */
	    bzero(buf, BUFSIZE);
	    n = recvfrom(sockfd, buf, BUFSIZE, 0, &serveraddr, &serverlen);
	    if (n < 0) 
	     	error("ERROR in recvfrom");
	    printf("\nEcho from server:\n%s", buf);
    }

    return 0;
}
