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
#include <openssl/md5.h>

#define BUFSIZE   8192   /* max I/O buffer size */
#define LISTENQ  1024    /* second argument to listen() */

pthread_mutex_t h_lock; // host
pthread_mutex_t b_lock; // blacklist
pthread_mutex_t p_lock; // page

int open_listenfd(int port);
void webproxy(int connfd);
void *thread(void *vargp);
int check_host_cache(char *host, char **address);
int check_blacklist(char *host, char *ip);
char *hash(char *str, int length);

int main(int argc, char **argv) 
{
  int listenfd, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
  struct sockaddr_in clientaddr;
  pthread_t tid; 

  if (argc < 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(0);
  }
  port = atoi(argv[1]);

  // open client port
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
  webproxy(connfd);
  close(connfd);
  return NULL;
}

/* thread function */
void webproxy(int connfd) 
{
	size_t n; 
  char buf[BUFSIZE]; 
  char tmp_buf[BUFSIZE];
  char msg[BUFSIZE];
  struct hostent *hostinfo;
  int s_port;
  int host_cache_ret;

	// get request
  n = read(connfd, buf, BUFSIZE);
  printf("Server received the following request:\n%s\n",buf);

  strcpy(tmp_buf, buf);

  char *request = strtok(tmp_buf, "\n");
  char *rest = strtok(NULL, "");

  char *method = strtok(request, " ");
  char *url = strtok(NULL, " ");
  if (url) { // ensure got a request with a real url
  	char *orig_url[BUFSIZE];
  	strcpy(orig_url, url);
	  char *version = strtok(NULL, " ");

	  if (strstr(method, "GET") != NULL) { // it is a GET request

	  	if (strstr(url, "http://") != NULL) { // remove the http://
	  		memmove(url, url+7, strlen(url));
	  	}
	  	if (strstr(url, "https://") != NULL) { // remove the https://
	  		memmove(url, url+8, strlen(url));
	  	}

	  	// parse the hostname
	  	char *url_port = strtok(url, "/");
	  	char *path = strtok(NULL, "");

	  	if (!path) {
	  		path = "";
	  	}

	  	// take off port if it exists
	  	if (strstr(url_port, ":") != NULL) { // port is specified
				char *tmp = strtok(url_port, ":");
				char *p = strtok(NULL, ":");
				s_port = atoi(p);
			}
			else { // no port, default to 80
				s_port = 80;
			}

			// hostinfo = gethostbyname(url_port);

			char *addr;
			struct sockaddr_in sin;
			memset(&sin, 0, sizeof(sin));
			sin.sin_family = AF_INET;
	    sin.sin_port = htons(s_port);

			host_cache_ret = check_host_cache(url_port, &addr);


			if (host_cache_ret == 0) {

				hostinfo = gethostbyname(url_port);

				// mutex
				pthread_mutex_lock(&h_lock);

				FILE *fp;

				if (fp = fopen("host_cache.txt", "ab")) {
					fprintf(fp, "%s,%s\n", url_port, inet_ntoa(*(struct in_addr*)hostinfo->h_addr));

					fclose(fp);
				}
				else {
					printf("Error opening host cache file.\n");
				}

				// mutex
				pthread_mutex_unlock(&h_lock);

				memcpy(&sin.sin_addr, hostinfo->h_addr, hostinfo->h_length);
			}
			else {
				sin.sin_addr.s_addr = inet_addr(addr);
			}

			if (host_cache_ret == 0 && !hostinfo) { // invalid host name
	  		bzero(msg, BUFSIZE);
	  		strcpy(msg, "404 Not Found\n");
	  		write(connfd, msg, strlen(msg));
	  	}
	  	else { // valid host name
	  		// check blacklist
				if (check_blacklist(url_port, inet_ntoa(sin.sin_addr)) == 0) {

					char *hash_out = hash(orig_url, strlen(orig_url));

					// check page cache
					FILE *p_fp;
					char *line = NULL;
					size_t size = 0;
					int cached = 0;

					// mutex
					pthread_mutex_lock(&p_lock);

					if (p_fp = fopen("page_cache.txt", "ab+")) {
						while (getline(&line, &size, p_fp) != -1) {
							if (line[strlen(line)-1] == '\n') { // remove newline
								line[strlen(line)-1] = '\0';
							}
							if ((strcmp(line, hash_out) == NULL)) { // match
								cached = -1;
							}
						}
						if (cached == 0) {
							fprintf(p_fp, "%s\n", hash_out);
						}
						fclose(p_fp);
					}
					else {
						printf("Error opening page cache file.\n");
					}
					// mutex
					pthread_mutex_unlock(&p_lock);


					if (cached == 0) { // page is not cached, open socket
			  		// open socket here
			  		int sockfd, n;

			  		sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
				    if (sockfd < 0) {
				    	error("Error opening socket.");
				    }
				    else {
					    /* build the server's Internet address */
							if (connect(sockfd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
								printf("Error connecting to server.\n");
							}
							else {
								bzero(buf, BUFSIZE);
								sprintf(buf, "%s /%s %s\n%s\r\n\r\n", method, path, version, rest);

						    n = write(sockfd, buf, strlen(buf));
						    bzero(buf, BUFSIZE);

						    FILE *p_fp;
						    char *cache_path[39];
						    int bytes = 0;
								sprintf(cache_path, "cache/%s", hash_out);

						    if (p_fp = fopen(cache_path, "ab+")) {
							    while ((n = read(sockfd, buf, BUFSIZE)) > 0) {
								    send(connfd, buf, n, MSG_NOSIGNAL);
								    bytes = fwrite(buf, 1, n, p_fp);
								    bzero(buf, BUFSIZE);
							    }
							    fclose(p_fp);
							  }
							  else {
							  	printf("Error opening cached file: %s.\n", hash_out);
							  }
						  }
						}
					}
					else { // page is cached, go look it up
						FILE *p_fp;
						char *line = NULL;
						size_t size = 0;
						char *cache_path[39];
						sprintf(cache_path, "cache/%s", hash_out);

						if (p_fp = fopen(cache_path, "rb")) {

							bzero(buf, BUFSIZE);
							while ((n = fread(buf, 1, BUFSIZE, p_fp)) > 0) {
								send(connfd, buf, n, MSG_NOSIGNAL);
								bzero(buf, BUFSIZE);
							}

							fclose(p_fp);
						}
						else {
							printf("Error opening page cache file.\n");
						}
					}

				}
				else {
					bzero(msg, BUFSIZE);
			  	strcpy(msg, "ERROR 403 Forbidden\n");
			  	write(connfd, msg, strlen(msg));
				}
	  	}
	  }
	  else { // not a GET request
	  	bzero(msg, BUFSIZE);
	  	strcpy(msg, "400 Bad Request\n");
	  	write(connfd, msg, strlen(msg));
	  }
	}
	return NULL;
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
    // if (setsockopt(listenfd, SOL_SOCKET, SO_NOSIGPIPE, (const void *)&optval , sizeof(int)) < 0)
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


/* 
 * check_host_cache - open the hostname/IP file to see if host has been cached
 * if not cached, returns 0
 * if cached, returns -1
 */
int check_host_cache(char *host, char **address) {
	int ret = 0;
	FILE *fp;
	char *line = NULL;
	char *h, *a = NULL;
	size_t size = 0;
	ssize_t bytes;

	// mutex
	pthread_mutex_lock(&h_lock);

	if (fp = fopen("host_cache.txt", "rb")) {
		while (getline(&line, &size, fp) != -1) {

			strtok(line, ",");
			a = strtok(NULL, "\n");

			if ((strcmp(line, host) == NULL)) { // match
				*address = a;
				ret = -1;
				break;
			}
		}

		fclose(fp);
	}
	else {
		printf("Error opening host cache file.\n");
	}
	// mutex
	pthread_mutex_unlock(&h_lock);

	return ret;
}



/*
 * check_blacklist - checks to see if requested host or IP is in the blacklist
 * returns -1 if it is blacklisted
 * returns 0 if it is not
 */
int check_blacklist(char *host, char *ip) {
	FILE *fp;
	char *line = NULL;
	size_t size = 0;
	int ret = 0;

	pthread_mutex_lock(&b_lock);

	if (fp = fopen("blacklist.txt", "rb")) {
		while (getline(&line, &size, fp) > 0) {
			
			if (line[strlen(line)-1] == '\n') { // remove newline
				line[strlen(line)-1] = '\0';
			}
			
			if ((strcmp(line, host) == NULL) || (strcmp(line, ip) == NULL)) { // if IP or host matches
				ret = -1;
			}

		}
		fclose(fp);
	}
	else {
		printf("Error opening blacklist file.\n");
	}

	pthread_mutex_unlock(&b_lock);

	return ret;
}

char *hash(char *str, int length) {
	int q;
  MD5_CTX c;
  unsigned char digest[16];
  char *out = (char*)malloc(33);

  MD5_Init(&c);

  while (length > 0) {
    if (length > 512) {
      MD5_Update(&c, str, 512);
    } else {
      MD5_Update(&c, str, length);
    }
    length -= 512;
    str += 512;
  }

  MD5_Final(digest, &c);

  for (q = 0; q < 16; ++q) {
    snprintf(&(out[q*2]), 16*2, "%02x", (unsigned int)digest[q]);
  }

	return out;
}



