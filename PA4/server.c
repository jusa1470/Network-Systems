#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#define BUFSIZE 1024
#define LISTENQ  1024

char *dir = NULL;

int open_sockfd(int port);
void *thread(void * vargp);
void server(int connfd);
int check_cred(char *u, char *p);
void list(int connfd, char *user_dir);
void get(int connfd, char *user_dir);
void put(int connfd, char *user_dir);


int main(int argc, char **argv) {
	char *user = NULL;
	char *password = NULL;
	int sockfd, *connfdp, clientlen=sizeof(struct sockaddr_in);
	struct sockaddr_in clientaddr;
  pthread_t tid;
  int optval = 1;

	/* check command line arguments */
  if (argc != 3) {
  	fprintf(stderr,"usage: %s <directory> <port>\n", argv[0]);
  	exit(0);
  }

  dir = argv[1];
  int port = atoi(argv[2]);

	sockfd = open_sockfd(port);

	while (1) {
		connfdp = malloc(sizeof(int));
		*connfdp = accept(sockfd, (struct sockaddr*)&clientaddr, &clientlen);
		pthread_create(&tid, NULL, thread, connfdp);
	}
}


/*
	open socket to servers
*/
int open_sockfd(int port) 
{
	int sockfd, optval=1;
	struct sockaddr_in serveraddr;

	/* Create a socket descriptor */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	/* Eliminates "Address already in use" error from bind. */
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0)
		return -1;

	/* build the server's Internet address */
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET; 
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
	serveraddr.sin_port = htons((unsigned short)port); 

	if (bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
		return -1;

	if (listen(sockfd, LISTENQ) < 0)
		return -1;

	return sockfd;
}



/* 
 * thread routine
 */
void * thread(void * vargp) 
{  
  int connfd = *((int *)vargp);
  pthread_detach(pthread_self()); 
  free(vargp);
  char *buf[BUFSIZE];
  server(connfd);
  close(connfd);
  return NULL;
}



/*
 * thread function
 */
void server(int connfd) {
	char *buf[BUFSIZE];
	char *user_tmp[100];
	char *user = NULL, *password = NULL;
	int cred;

	// receive, parse, and verify credentials from client
	read(connfd, buf, BUFSIZE);

	user = strtok(buf, " ");
	password = strtok(NULL, "\n");
	strcpy(user_tmp, user);

	// check the user and password credentials and send whether it's valid to client
	cred = check_cred(user, password);
	bzero(buf, BUFSIZE);

	if (cred == 0) {
		strcpy(buf, "Credentials are valid.\n");
	}
	else {
		strcpy(buf, "Invalid Username/Password. Please try again.\n");
	}

	send(connfd, buf, BUFSIZE, MSG_NOSIGNAL);

	// check if user directory exists
	char *user_dir[100];
	sprintf(user_dir, ".%s/%s", dir, user_tmp);
	DIR *dp = opendir(user_dir);

	if (dp) { // directory exists, close it
		closedir(dp);
	}
	else { // directory does not exist, create it
		int result = mkdir(user_dir, 0777);
		if (result) {
			printf("Error creating user directory.\n");
		}
	}

	int size;
	while (1) {
		bzero(buf, BUFSIZE);
		read(connfd, (char*)&size, sizeof(size));
		read(connfd, buf, size);

		printf("command: %s\n", buf);
		if (strstr(buf, "list") != NULL) {
			list(connfd, user_dir);
		}
		else if (strstr(buf, "get") != NULL) {
			get(connfd, user_dir);
		}
		else if (strstr(buf, "put") != NULL) {
			put(connfd, user_dir);
		}
	}
}



/* 
 * parse the dfs.conf file for username and password
 * returns 0 if credentials are valid
 * returns -1 if credentials are invalid
 */
int check_cred(char *u, char *p) {
	FILE *fp;
	char *line = NULL;
	size_t size = 0;
	char *uu = NULL, *pp = NULL;
	int ret = -1;

	if (fp = fopen("dfs.conf", "rb")) {
		while (getline(&line, &size, fp) != -1) {

			uu = strtok(line, " ");
			pp = strtok(NULL, "\n");

			if (strcmp(u, uu) == NULL) {
				if (strcmp(p, pp) == NULL) {
					ret = 0;
					break;
				}
			}
		}
		fclose(fp);
	}
	else {

		printf("Error opening dfc.conf file.\n");
	}

	return ret;
}




void list(int connfd, char *user_dir) {
	char *buf[BUFSIZE];
	DIR *dp; 
	struct dirent *d;
	dp = opendir(user_dir);
	int size;
	if (dp) {
		while ((d = readdir(dp)) != NULL) {
			bzero(buf, BUFSIZE);
			strcpy(buf, d->d_name);
			if (strlen(buf) > 2) {
				size = strlen(buf);
				write(connfd, (char*)&size, sizeof(size));
				write(connfd, buf, size);
			}
		}
		closedir(dp);
	}
	bzero(buf, BUFSIZE);
	strcpy(buf, "done");
	size = strlen(buf);
	write(connfd, (char*)&size, sizeof(size));
	write(connfd, buf, size);
}




void get(int connfd, char *user_dir) {
	char *buf[BUFSIZE];
	int size;
	int i;
	int match = 0;
	FILE *fp;

	// send file piece names
	bzero(buf, BUFSIZE);
	read(connfd, (char*)&size, sizeof(size));
	read(connfd, buf, size);
	char *filename[100];
	strcpy(filename, buf);

	DIR *dp;
	struct dirent *d;
	dp = opendir(user_dir);
	if (dp) {
		while ((d = readdir(dp)) != NULL) {
			bzero(buf, BUFSIZE);
			strcpy(buf, d->d_name);
			if (strstr(buf, filename) != NULL) {
				size = strlen(buf);
				write(connfd, (char*)&size, sizeof(size));
				write(connfd, buf, size);
			}
		}
		closedir(dp);
	}
	bzero(buf, BUFSIZE);
	strcpy(buf, "done");
	size = strlen(buf);
	write(connfd, (char*)&size, sizeof(size));
	write(connfd, buf, size);

	// see if server should check for file
	bzero(buf, BUFSIZE);
	read(connfd, (char*)&size, sizeof(size));
	read(connfd, buf, size);

	if (strcmp(buf, "go") == NULL) {
		// send file piece if available
		for (i = 0; i < 4; i++) {
			bzero(buf, BUFSIZE);
			read(connfd, (char*)&size, sizeof(size));
			read(connfd, buf, size);
			if (strcmp(buf, "stop") == NULL) {
				continue;
			}
			char *path[200];
			sprintf(path, "%s/%s", user_dir, buf);
			printf("path: %s\n", path);
			if (fp = fopen(path, "rb")) {
				bzero(buf, BUFSIZE);
				strcpy(buf, "available");
				size = strlen(buf);
				write(connfd, (char*)&size, sizeof(size));
				write(connfd, buf, size);

				int f_size;
				fseek(fp, 0L, SEEK_END);
				f_size = ftell(fp);
				fseek(fp, 0L, SEEK_SET);

				write(connfd, (char*)&f_size, sizeof(f_size));

				float bufs_to_send = f_size / (float)BUFSIZE;
				int bytes_read = 0;
				while (bufs_to_send > 1) {
					bzero(buf, BUFSIZE);
					bytes_read += fread(buf, 1, BUFSIZE, fp);
					write(connfd, buf, strlen(buf));
					bufs_to_send -= 1;
				}
				int last_num_bytes = f_size - bytes_read;
				bzero(buf, BUFSIZE);
				fread(buf, 1, last_num_bytes, fp);
				write(connfd, buf, last_num_bytes);
			}
			else {
				bzero(buf, BUFSIZE);
				strcpy(buf, "unavailable");
				size = strlen(buf);
				write(connfd, (char*)&size, sizeof(size));
				write(connfd, buf, size);
			}
		}
	}
	else {
		printf("NO GO\n");
	}
}





void put(int connfd, char *user_dir) {
	char *buf[BUFSIZE];
	char *tmp_buf = "";
	char *content = "";;
	FILE *fp1, *fp2;
	int bytes = 0;

	int name_size1, name_size2, piece_size1, piece_size2;

	char *filename1[100];
	read(connfd, buf, BUFSIZE);
	strcpy(filename1, buf);
	bzero(buf, BUFSIZE);
	char *path1[strlen(user_dir) + name_size1];
	sprintf(path1, "%s/.%s", user_dir, filename1);

	if (fp1 = fopen(path1, "ab+")) {
		// get content size
		read(connfd, (char*)&piece_size1, sizeof(piece_size1));
		int bytes_read = piece_size1;
		if (piece_size1 > BUFSIZE) {
			while(bytes_read > BUFSIZE) {
				bzero(buf, BUFSIZE);
				bytes = read(connfd, buf, BUFSIZE);
				bytes_read = bytes_read - bytes;
				fwrite(buf, 1, bytes, fp1);
			}
		}
		bzero(buf, BUFSIZE);
		read(connfd, buf, bytes_read);
		fwrite(buf, 1, bytes_read, fp1);
		fclose(fp1);
	}

	// get filename size and filename for part 2
	bzero(buf, BUFSIZE);
	bytes = 0;
	char *filename2[100];
	read(connfd, buf, BUFSIZE);
	strcpy(filename2, buf);
	bzero(buf, BUFSIZE);
	char *path2[strlen(user_dir) + name_size2];

	sprintf(path2, "%s/.%s", user_dir, filename2);
	if (fp2 = fopen(path2, "ab+")) {
		// get content size
		read(connfd, (char*)&piece_size2, sizeof(piece_size2));
		int bytes_read = piece_size2;
		if (piece_size2 > BUFSIZE) {
			while(bytes_read > BUFSIZE) {
				bzero(buf, BUFSIZE);
				bytes = read(connfd, buf, BUFSIZE);
				bytes_read = bytes_read - bytes;
				fwrite(buf, 1, bytes, fp2);
			}
		}
		bzero(buf, BUFSIZE);
		read(connfd, buf, bytes_read);
		fwrite(buf, 1, bytes_read, fp2);
		fclose(fp2);
	}

}