/* 
 * tcpechosrv.c - A concurrent TCP echo server using threads
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>      /* for opendir */

#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */

int open_listenfd(int port);
void echo(int connfd);
void *thread(void *vargp);

int main(int argc, char **argv) 
{
  int listenfd, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
  struct sockaddr_in clientaddr;
  pthread_t tid; 

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(0);
  }
  port = atoi(argv[1]);

  listenfd = open_listenfd(port);
  while (1) {
    connfdp = malloc(sizeof(int));
    *connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
    pthread_create(&tid, NULL, thread, connfdp);
  }
}

/* thread routine */
void * thread(void * vargp) 
{  
  int connfd = *((int *)vargp);
  pthread_detach(pthread_self()); 
  free(vargp);
  echo(connfd);
  close(connfd);
  return NULL;
}

/*
 * echo - read and echo text lines until client closes connection
 */
void echo(int connfd) 
{
  size_t n; 
  char buf[MAXLINE]; 
  char *req;
  char *ver;
  char content_type[15];
  char header[MAXBUF];
  FILE *fp;
  int bytes_read;
  int file_size;
  int file_close = 0;
  
  // get request
  n = read(connfd, buf, MAXLINE);
  printf("server received the following request:\n%s\n",buf);

  // parse the get request with the request and the version
  char *get = strtok(buf, "\n");
  char *tmp = strtok(get, " ");
  tmp = strtok(NULL, " ");
  req = tmp;
  tmp = strtok(NULL, " ");
  ver = tmp;

  // status code and version templates
  char error[] = "500 Internal Server Error";
  char ok[] = "200 Document Follows";
  char version[8];

  // get the right version
  if (ver != NULL) {
	  if (strcmp(ver, "HTTP/1.0") == 0) {
	  	strcpy(version, "HTTP/1.0");
	  }
	  else {
	  	strcpy(version, "HTTP/1.1");
	  }
	}

  if (req != NULL) { // make sure there is a request
	  if (strcmp(req, "/") == 0 || strcmp(req, "/inside/") == 0) { // check if the request is the default page
	    if (fp = fopen("index.html", "rb")) { // try to open the index.html file
	      // create a tmp_buf to read in the info and put it into the header with correct credentials
	      char tmp_buf[MAXBUF];
	      bytes_read = fread(tmp_buf, 1, MAXBUF, fp);
	      fclose(fp);
	      sprintf(header, "%s %s\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s", version, ok, bytes_read, tmp_buf);
	    }
	    else { // error if index.html doesn't open
	      char msg[] = "Index.html failed to open.\n";
	      sprintf(header, "%s %s\r\nContent-Type: error\r\nContent-Length: %d\r\n\r\n%s", version, error, strlen(msg), msg);
	    }
	  }
	  else { // if it's not the default page
	  	// remove leading / or /inside/
	  	if (strstr(req, "/inside/") != NULL) {
	  		memmove(req, req+8, strlen(req));
	  	}
	  	else {
		    memmove(req, req+1, strlen(req));
			}

	    // look for content type
	    if (strstr(req, ".html") != NULL) {
	    	strcpy(content_type, "text/html");
	    }
	    else if (strstr(req, ".txt") != NULL) {
	    	strcpy(content_type, "text/plain");
	    }
	    else if (strstr(req, ".png") != NULL) {
	    	strcpy(content_type, "image/png");
	    }
	    else if (strstr(req, ".gif") != NULL) {
	    	strcpy(content_type, "image/gif");
	    }
	    else if (strstr(req, ".jpg") != NULL) {
	    	strcpy(content_type, "image/jpg");
	    }
	    else if (strstr(req, ".css") != NULL) {
	    	strcpy(content_type, "text/css");
	    }
	    else if (strstr(req, ".js") != NULL) {
	    	strcpy(content_type, "application/javascript");
	    }

	    printf("request: %s\n", req);

	    if (fp = fopen(req, "rb")) { // try to open the file
	    	// find length of file
	    	fseek(fp, 0, SEEK_END);
	    	file_size = ftell(fp);
	    	fseek(fp, 0, SEEK_SET);

	      // set up header
	    	bzero(header, MAXBUF);
	      sprintf(header, "%s %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n", version, ok, content_type, file_size);
	      // file opened successfully, so set a bit to remind to close the file later
	      file_close = 1;
	    }
	    else { // if the file didn't open or doesn't exist - send an error message
	      char msg[] = "Incorrect file name.";
	      sprintf(header, "%s %s\r\nContent-Type: error\r\nContent-Length: %d\r\n\r\n%s", version, error, strlen(msg), msg);
	    }
	  }

	  // send the header
	  printf("server returning a http message with the following content.\n%s\n",header);
	  write(connfd, header, strlen(header));

	  // send file content
		while (file_size != 0) {
			// read into buffer
			bzero(buf, MAXBUF);
	    bytes_read = fread(buf, 1, MAXBUF, fp);
	    // decrease file size to know how many bytes are left
	    file_size = file_size - bytes_read;
		  write(connfd, buf, bytes_read);
		}

		if (file_close) { // if file opened, close bit is set, so close the file
	  	fclose(fp);
	  }
	}
}

/* 
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure 
 */
int open_listenfd(int port) 
{
  int listenfd, optval=1;
  struct sockaddr_in serveraddr;

  /* Create a socket descriptor */
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      return -1;

  /* Eliminates "Address already in use" error from bind. */
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0)
      return -1;

  /* listenfd will be an endpoint for all requests to port
     on any IP address for this host */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET; 
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
  serveraddr.sin_port = htons((unsigned short)port); 
  if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
      return -1;

  /* Make it a listening socket ready to accept connection requests */
  if (listen(listenfd, LISTENQ) < 0)
      return -1;
  return listenfd;
} /* end open_listenfd */
