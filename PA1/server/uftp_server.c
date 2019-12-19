#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");

    /* 
     * gethostbyaddr: determine who sent the datagram
     */
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("server received datagram from %s (%s)\n", hostp->h_name, hostaddrp);
    printf("server received %d/%d bytes\n", strlen(buf), n);

    /*-----------Handle Commands-----------*/
    /* get command */
    if (strstr(buf, "get") != NULL) {
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
          sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);

          int bytes_read;
          do { // begin sending data
            bzero(buf, BUFSIZE);
            bytes_read = fread(buf, 1, BUFSIZE, file);
            sendto(sockfd, buf, bytes_read, 0, (struct sockaddr *) &clientaddr, clientlen);
          } while (bytes_read == BUFSIZE);

          fclose(file);
          bzero(buf, BUFSIZE);
          strcpy(buf, "File get successful.\n");
        }
        else { // if file does not exist
          bzero(buf, BUFSIZE);
          strcpy(buf, "File does not exist.\n");
          sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);

          bzero(buf, BUFSIZE);
          strcpy(buf, "File does not exist on server side.\n");
        }
      }
      else { // filename not in command
        bzero(buf, BUFSIZE);
        strcpy(buf, "Filename not in command. Please re-enter command with a filename.\n");
      }
    }

    /* put command */
    else if (strstr(buf, "put") != NULL) {
      // parse filename from command
      char buf2[BUFSIZE];
      strcpy(buf2, buf);
      char *token = strtok(buf2, " ");
      token = strtok(NULL, " ");

      if (token != NULL) { // if filename is in the command
        // strip newline and create file pointer
        token[strlen(token)-1] = '\0';
        FILE *file;

        // handshake to make sure file exists
        bzero(buf, BUFSIZE);
        recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);

        if (strcmp(buf, "File exists.\n") == 0) { // if file exists
          if (file = fopen(token, "wb")) { // if file opens/creates seccessfully
            // begin receiving file contents
            int nbytes, bytes_written;

            do { // begin receiving data
              bzero(buf, BUFSIZE);
              nbytes = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
              if (buf[nbytes-1] == EOF) { // if last char is EOF
                nbytes = nbytes - 1; // don't write last EOF char
              }
              bytes_written = fwrite(buf, 1, nbytes, file);
            } while (bytes_written == BUFSIZE);

            fclose(file);
            bzero(buf, BUFSIZE);
            strcpy(buf, "File put successful.\n");
          }
          else { // if file fails to create/open file
            bzero(buf, BUFSIZE);
            strcpy(buf, "Failed to create new file on the server side.\n");
          }
        }
        else if (strcmp(buf, "File does not exist.\n") == 0) { // if file does not exist
          bzero(buf, BUFSIZE);
          strcpy(buf, "File does not exist on the client side.\n");
        }
      }
      else { // filename is not in command
        bzero(buf, BUFSIZE);
        strcpy(buf, "No file name found. Please enter the put command again with the name of a file.\n");
      }
    }

    /* delete command */
    else if (strstr(buf, "delete") != NULL) {
      // parse filename from input
      char *token = strtok(buf, " ");
      token = strtok(NULL, " ");

      if (token != NULL) { // if filename is in the command
        // strip newline and create file pointer
        token[strlen(token)-1] = '\0';
        FILE *file;

        if (file = fopen(token, "r")) { // if file exists
          fclose(file);
          // delete
          int status = remove(token);

          if (status == 0) { // if delete was successful
            char *msg = "The file was deleted successfully.\n";
            strcpy(buf, msg);
          }
          else { // if delete was not successful
            bzero(buf, BUFSIZE);
            char *err_msg = "Something went wrong. The delete was not successful.\n";
            strcpy(buf, err_msg);
          }
        }
        else { // if file does not exist
          bzero(buf, BUFSIZE);
          char *err_msg = "File does not exist. Please enter an existing file name.\n";
          strcpy(buf, err_msg);
        }
      }
      else { // if no filename was in the command
        bzero(buf, BUFSIZE);
        char *err_msg = "No file name found. Please enter the get command again with the name of a file.\n";
        strcpy(buf, err_msg);
      }
    }

    /* ls command */
    else if (strcmp(buf, "ls\n") == 0) {
      FILE *output = popen(buf, "r");
      bzero(buf, BUFSIZE);
      char p;
      while ((p = fgetc(output)) != EOF) {
        buf[strlen(buf)] = p;
      }
    }

    /* exit command */
    else if (strcmp(buf, "exit\n") == 0) {
      exit(0);
    }

    /* no recognized command*/
    else {
      char *err_msg = "This command is not found. Please enter a new command from the menu.\n";
      strcpy(buf, err_msg);
    }
    
    /* 
     * sendto: echo the input back to the client 
     */
    n = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
    if (n < 0) 
      error("ERROR in sendto");
  }
}
