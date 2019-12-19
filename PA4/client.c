#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h> 
#include <sys/sendfile.h>
#include <openssl/md5.h>

#define BUFSIZE 1024
#define LISTENQ  1024  /* second argument to listen() */

void parse(char *f, int *port, char **u, char **p);
int open_sockfd(int port);
int check_cred(int sockfd, char *user, char *password);
void list(int *sockfds);
void get(int *sockfds, char *filename);
void put(int *sockfds, char *filename);
char *hash(char *str, int length);


int main(int argc, char **argv) {
	int s_ports[4];
	char *user = "";
	char *password = "";
	int sockfds[4];
	int i;
	char *buf[BUFSIZE];
	int serverlen;
	FILE *fp;

	/* check command line arguments */
  if (argc != 2) {
  	fprintf(stderr,"usage: %s <configuration file>\n", argv[0]);
  	exit(0);
  }

  char *conf_file = argv[1];

  // parse dfc.conf file
  parse(conf_file, s_ports, &user, &password);
  //printf("conf_file: %s\ns_ports: %d %d %d %d\nuser: %s\npassword: %s\n", conf_file, s_ports[0], s_ports[1], s_ports[2], s_ports[3], user, password);

  for (i = 0; i < 4; i++) {
  	if ((sockfds[i] = open_sockfd(s_ports[i])) == -1) {
  		printf("Error connecting to server on port %d.\n", s_ports[i]);
  	}
  	else {
  		printf("Successful connection to server on port %d.\n", s_ports[i]);
  	}
  }

  int cred = -1;
  // send credentials to first available server
  for (i = 0; i < 4; i++) {
  	if (sockfds[i] != -1) {
  		cred = check_cred(sockfds[i], user, password);
		  if (cred != 0) {
		  	printf("Invalid Username/Password. Please try again.\n");
		  	cred = 1;
		  }
  	}
  }

  // print menu 
  int send;
  while (1) {
  	send = 1;
		printf("\n-----MENU-----\n");
		printf("1. list\n");
		printf("2. get [file_name]\n");
		printf("3. put [file_name]\n");

		/* get a message from the user */
		bzero(buf, BUFSIZE);
		printf("Enter command: ");
		fgets(buf, BUFSIZE, stdin);

		// Handle Commands //

		/* list */
		if (strcmp(buf, "list\n") == NULL) {
			buf[strlen(buf)-1] = '\0';
			for (i = 0; i < 4; i++) {
				if (sockfds[i] != -1) {
					int size = strlen(buf);
					write(sockfds[i], (char*)&size, sizeof(size));
					int n = write(sockfds[i], buf, size);
					if (n < 0) 
						error("ERROR in write");
				}
			}
			list(sockfds);
		}
		/* get */
		else if (strstr(buf, "get") != NULL) {
			char *tmp = strtok(buf, " ");
			char *filename = strtok(NULL, "\n");
			if (!filename) {
				printf("usage: get <filename>\n");
				send = 0;
			}
			if (send) {
				for (i = 0; i < 4; i++) {
					if (sockfds[i] != -1) {
						int size = strlen(buf);
						write(sockfds[i], (char*)&size, sizeof(size));
						int n = write(sockfds[i], buf, size);
						if (n < 0) 
							error("ERROR in write");
					}
				}
				get(sockfds, filename);
			}
		}
		/* put */
		else if (strstr(buf, "put") != NULL) {
			char *tmp = strtok(buf, " ");
			char *filename = strtok(NULL, "\n");
			if (!filename) {
				printf("usage: put <filename>\n");
				send = 0;
			}
			if (fp = fopen(filename, "r")) {
				fclose(fp);
			}
			else {
				send = 0;
			}
			if (send) {
				for (i = 0; i < 4; i++) {
					if (sockfds[i] != -1) {
						int size = strlen(buf);
						write(sockfds[i], (char*)&size, sizeof(size));
						int n = write(sockfds[i], buf, size);
						if (n < 0) 
							error("ERROR in write");
					}
				}
				put(sockfds, filename);
			}
		}
		else {
			printf("Incorrect command. See menu.\n");
		}
	}
}



/* 
	parse the dfc.conf file for ports, username, and password
*/
void parse(char *f, int *port, char **u, char **p) {
	FILE *fp;
	char *line = NULL;
	char *line2 = NULL;
	size_t size = 0;
	char *tmp = NULL;
	char *p_tmp = NULL;
	int i = 0;

	if (fp = fopen(f, "rb")) {
		for (i = 0; i < 4; i++) {
			getline(&line, &size, fp);
			tmp = strtok(line, ":");
			p_tmp = strtok(NULL, "\n");
			port[i] = atoi(p_tmp);
		}

		getline(&line, &size, fp);
		tmp = strtok(line, " ");
		*u = strtok(NULL, "\n");

		getline(&line2, &size, fp);
		tmp = strtok(line2, " ");
		*p = strtok(NULL, "");
	}
	else {
		printf("Error opening dfc.conf file.\n");
	}
}



/*
	open socket to servers
*/
int open_sockfd(int port) {
  int sockfd, optval=1;
  struct sockaddr_in serveraddr;
  struct timeval timeout;
  timeout.tv_sec = 1;

  /* Create a socket descriptor */
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      return -1;

  /* Sets timeout value. */
  // if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(int)) < 0)
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int)) < 0)
      return -1;

  /* build the server's Internet address */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET; 
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
  serveraddr.sin_port = htons((unsigned short)port); 

  if (connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
		return -1;
	}

  return sockfd;
}



/*
 * send credentials to server
 * receive confirmation from server if credentials are valid
 * returns 0 if credentials are valid
 * returns -1 if credentials are invalid
 */
int check_cred(int sockfd, char *u, char *p) {
	char *buf[BUFSIZE];
	sprintf(buf, "%s %s", u, p);
	write(sockfd, buf, BUFSIZE);
	bzero(buf, BUFSIZE);
	read(sockfd, buf, BUFSIZE);

	if (strcmp(buf, "Credentials are valid.\n") == NULL) {
		return 0;
	}
	else {
		return -1;
	}
}




/*
 * list funciton
 * identify if pieces are enough
 * print files listed on server
 */
void list(int *sockfds) {
	char *buf[BUFSIZE];
	char files[100][100];
	char unique_files[100][100];
	int pieces[100][4];
	int i, j, size;
	int file_count = 0;
	char *tmp = "";

	for (i = 0; i < 100; i++) {
		files[i][0] = '\0';
		unique_files[i][0] = '\0';
		pieces[i][0] = -1;
		pieces[i][1] = -1;
		pieces[i][2] = -1;
		pieces[i][3] = -1;
	}

	for (i = 0; i < 4; i++) {
		bzero(buf, BUFSIZE);
		if (sockfds[i] != -1) {
			read(sockfds[i], (char*)&size, sizeof(size));
			read(sockfds[i], buf, size);
			while (strcmp(buf, "done") != NULL) {
				if (strlen(buf) > 0) {
					strcpy(files[file_count], buf);
					memmove(files[file_count], files[file_count]+1, strlen(files[file_count]));
					file_count++;
				}
				bzero(buf, BUFSIZE);
				read(sockfds[i], (char*)&size, sizeof(size));
				read(sockfds[i], buf, size);
			}
		}
	}

	int u_file_count = 0;
	int found = 0;

	for (i = 0; i < 100; i++) {
		found = 0;
		if (files[i][0] != '\0') {
			for (int j = 0; j < 100; j++) {
				if (unique_files[j][0] != '\0') {
					char num = files[i][strlen(files[i])-1];
					files[i][strlen(files[i])-1] = '\0';
					if (strcmp(files[i], unique_files[j]) == NULL) {
						found = 1;
					}
					files[i][strlen(files[i])] = num;
					files[i][strlen(files[i])] = '\0';
				}
				else {
					break;
				}
			}
			if (found == 0) {
				strcpy(unique_files[u_file_count], files[i]);
				unique_files[u_file_count][strlen(unique_files[u_file_count])-1] = '\0';
				u_file_count++;
			}
		}
		else {
			break;
		}
	}

	for (i = 0; i < 100; i++) {
		if (files[i][0] != '\0') {
			for (j = 0; j < 100; j++) {
				if (unique_files[j][0] != '\0') {
					char num = files[i][strlen(files[i])-1];
					files[i][strlen(files[i])-1] = '\0';
					if (strcmp(files[i], unique_files[j]) == NULL) {
						if (num == '1') {
							pieces[j][0] = 1;
						}
						else if (num == '2') {
							pieces[j][1] = 1;
						}
						else if (num == '3') {
							pieces[j][2] = 1;
						}
						else if (num == '4') {
							pieces[j][3] = 1;
						}
					}
					files[i][strlen(files[i])] = num;
					files[i][strlen(files[i])] = '\0';
				}
			}
		}
		else {
			break;
		}
	}

	for (i = 0; i < 100; i++) {
		if (unique_files[i][0] != '\0') {
			unique_files[i][strlen(unique_files[i])-1] = '\0';
			if ((pieces[i][0] == 1) && (pieces[i][1] == 1) && (pieces[i][2] == 1) && (pieces[i][3] == 1)) {
				printf("%s\n", unique_files[i]);
			}
			else {
				printf("%s [incomplete]\n", unique_files[i]);
			}
		}
		else {
			break;
		}
	}
}



/*
 * get function
 */
void get(int *sockfds, char *filename) {
	char *buf[BUFSIZE];
	FILE *fp;
	int size, i;
	char *file_piece[100];
	int found = 0;
	char files[8][100];
	int file_count = 0;
	int pieces[4];
	char *path[200];
	sprintf(path, "./files/%s", filename);

	// initialize files
	for (i = 0; i < 8; i++) {
		files[i][0] = '\0';
	}

	// get all file names with numbers from servers
	for (i = 0; i < 4; i++) {
		if (sockfds[i] != -1) {
			strcpy(buf, filename);
			size = strlen(buf);
			write(sockfds[i], (char*)&size, sizeof(size));
			write(sockfds[i], buf, size);

			bzero(buf, BUFSIZE);
			read(sockfds[i], (char*)&size, sizeof(size));
			read(sockfds[i], buf, size);
			while (strcmp(buf, "done") != NULL) {
				strcpy(files[file_count], buf);
				file_count++;
				bzero(buf, BUFSIZE);
				read(sockfds[i], (char*)&size, sizeof(size));
				read(sockfds[i], buf, size);
			}
		}
		pieces[i] = -1;
	}

	// check if all pieces are there
	for (i = 0; i < 8; i++) {
		if (files[i][0] != '\0') {
			char num = files[i][strlen(files[i])-1];
			if (num == '1') {
				pieces[0] = 1;
			}
			else if (num == '2') {
				pieces[1] = 1;
			}
			else if (num == '3') {
				pieces[2] = 1;
			}
			else if (num == '4') {
				pieces[3] = 1;
			}
		}
	}

	if ((pieces[0] == 1) && (pieces[1] == 1) && (pieces[2] == 1) && (pieces[3] == 1)) {
		for (i = 0; i < 4; i++) {
			bzero(buf, BUFSIZE);
			strcpy(buf, "go");
			size = strlen(buf);
			write(sockfds[i], (char*)&size, sizeof(size));
			write(sockfds[i], buf, size);
		}

		// get file pieces from first available server that has each piece
		for (i = 0; i < 4; i++) {
			found = 0;
			sprintf(file_piece, ".%s.%d", filename, i+1);

			if (found == 0) {
				if (sockfds[0] != -1) {
					bzero(buf, BUFSIZE);
					strcpy(buf, file_piece);
					size = strlen(buf);
					write(sockfds[0], (char*)&size, sizeof(size));
					write(sockfds[0], buf, size);
					bzero(buf, BUFSIZE);
					read(sockfds[0], (char*)&size, sizeof(size));
					read(sockfds[0], buf, size);

					if (strcmp(buf, "available") == NULL) {
						if (fp = fopen(path, "ab+")) {
							found = 1;
							bzero(buf, BUFSIZE);
							read(sockfds[0], (char*)&size, sizeof(size));
							int bytes_read = size;
							if (size > BUFSIZE) {
								while (bytes_read > BUFSIZE) {
									bzero(buf, BUFSIZE);
									int bytes = read(sockfds[0], buf, BUFSIZE);
									bytes_read -= bytes;
									fwrite(buf, 1, bytes, fp);
								}
							}
							bzero(buf, BUFSIZE);
							read(sockfds[0], buf, bytes_read);
							fwrite(buf, 1, bytes_read, fp);
							fclose(fp);
						}
						else {
							printf("Error opening file.\n");
						}
					}
				}
			}
			else {
				strcpy(buf, "stop");
				size = strlen(buf);
				write(sockfds[0], (char*)&size, sizeof(size));
				write(sockfds[0], buf, size);
			}

			if (found == 0) {
				if (sockfds[1] != -1) {
					bzero(buf, BUFSIZE);
					strcpy(buf, file_piece);
					size = strlen(buf);
					write(sockfds[1], (char*)&size, sizeof(size));
					write(sockfds[1], buf, size);
					bzero(buf, BUFSIZE);
					read(sockfds[1], (char*)&size, sizeof(size));
					read(sockfds[1], buf, size);

					if (strcmp(buf, "available") == NULL) {
						if (fp = fopen(path, "ab+")) {
							found = 1;
							bzero(buf, BUFSIZE);
							read(sockfds[1], (char*)&size, sizeof(size));
							int bytes_read = size;
							if (size > BUFSIZE) {
								while (bytes_read > BUFSIZE) {
									bzero(buf, BUFSIZE);
									int bytes = read(sockfds[1], buf, BUFSIZE);
									bytes_read -= bytes;
									fwrite(buf, 1, bytes, fp);
								}
							}
							bzero(buf, BUFSIZE);
							read(sockfds[1], buf, bytes_read);
							fwrite(buf, 1, bytes_read, fp);
							fclose(fp);
						}
						else {
							printf("Error opening file.\n");
						}
					}
				}
			}
			else {
				strcpy(buf, "stop");
				size = strlen(buf);
				write(sockfds[1], (char*)&size, sizeof(size));
				write(sockfds[1], buf, size);
			}

			if (found == 0) {
				if (sockfds[2] != -1) {
					bzero(buf, BUFSIZE);
					strcpy(buf, file_piece);
					size = strlen(buf);
					write(sockfds[2], (char*)&size, sizeof(size));
					write(sockfds[2], buf, size);
					bzero(buf, BUFSIZE);
					read(sockfds[2], (char*)&size, sizeof(size));
					read(sockfds[2], buf, size);

					if (strcmp(buf, "available") == NULL) {
						if (fp = fopen(path, "ab+")) {
							found = 1;
							bzero(buf, BUFSIZE);
							read(sockfds[2], (char*)&size, sizeof(size));
							int bytes_read = size;
							if (size > BUFSIZE) {
								while (bytes_read > BUFSIZE) {
									bzero(buf, BUFSIZE);
									int bytes = read(sockfds[2], buf, BUFSIZE);
									bytes_read -= bytes;
									fwrite(buf, 1, bytes, fp);
								}
							}
							bzero(buf, BUFSIZE);
							read(sockfds[2], buf, bytes_read);
							fwrite(buf, 1, bytes_read, fp);
							fclose(fp);
						}
						else {
							printf("Error opening file.\n");
						}
					}
				}
			}
			else {
				strcpy(buf, "stop");
				size = strlen(buf);
				write(sockfds[2], (char*)&size, sizeof(size));
				write(sockfds[2], buf, size);
			}

			if (found == 0) {
				if (sockfds[3] != -1) {
					bzero(buf, BUFSIZE);
					strcpy(buf, file_piece);
					size = strlen(buf);
					write(sockfds[3], (char*)&size, sizeof(size));
					write(sockfds[3], buf, size);
					bzero(buf, BUFSIZE);
					read(sockfds[3], (char*)&size, sizeof(size));
					read(sockfds[3], buf, size);

					if (strcmp(buf, "available") == NULL) {
						if (fp = fopen(path, "ab+")) {
							found = 1;
							bzero(buf, BUFSIZE);
							read(sockfds[3], (char*)&size, sizeof(size));
							int bytes_read = size;
							if (size > BUFSIZE) {
								while (bytes_read > BUFSIZE) {
									bzero(buf, BUFSIZE);
									int bytes = read(sockfds[3], buf, BUFSIZE);
									bytes_read -= bytes;
									fwrite(buf, 1, bytes, fp);
								}
							}
							bzero(buf, BUFSIZE);
							read(sockfds[3], buf, bytes_read);
							fwrite(buf, 1, bytes_read, fp);
							fclose(fp);
						}
						else {
							printf("Error opening file.\n");
						}
					}
				}
			}
			else {
				strcpy(buf, "stop");
				size = strlen(buf);
				write(sockfds[3], (char*)&size, sizeof(size));
				write(sockfds[3], buf, size);
			}
		}
	}
	else {
		for (i = 0; i < 4; i++) {
			bzero(buf, BUFSIZE);
			strcpy(buf, "don't");
			write(sockfds[i], (char*)&size, sizeof(size));
			write(sockfds[i], buf, size);
		}
	}
}



/*
 * put function
 * reads file, divides it into 4 pieces, sends pieces to server
 */
void put(int *sockfds, char *filename) {
	char *buf[BUFSIZE];
	FILE *fp;
	int name_size;

	if (filename[strlen(filename)-1] == '\n') {
		filename[strlen(filename)-1] = '\0';
	}

	if (fp = fopen(filename, "rb")) {
		int bytes_read = 0;
		int f_size;
		fseek(fp, 0L, SEEK_END);
		f_size = ftell(fp);
		fseek(fp, 0L, SEEK_SET);

		bzero(buf, BUFSIZE);
		fread(buf, 1, BUFSIZE, fp);
		int h = atoi(hash(buf, strlen(buf))) % 4;
		printf("hash: %d\n", h);

		fseek(fp, 0L, SEEK_SET);

		bzero(buf, BUFSIZE);

		int piece = f_size / 4;
		int extra_bytes = f_size % 4;

		int conns[4];
		if (h == 0) {
			conns[0] = sockfds[0];
			conns[1] = sockfds[1];
			conns[2] = sockfds[2];
			conns[3] = sockfds[3];
		}
		else if (h == 1) {
			conns[0] = sockfds[1];
			conns[1] = sockfds[2];
			conns[2] = sockfds[3];
			conns[3] = sockfds[0];
		}
		else if (h == 2) {
			conns[0] = sockfds[2];
			conns[1] = sockfds[3];
			conns[2] = sockfds[0];
			conns[3] = sockfds[1];
		}
		else if (h == 3) {
			conns[0] = sockfds[3];
			conns[1] = sockfds[0];
			conns[2] = sockfds[1];
			conns[3] = sockfds[2];
		}


		// send part 1 to conns 0 and 3
		//************************************************************************//
		float bufs_to_send = piece / (float)BUFSIZE;
		bytes_read = 0;

		bzero(buf, BUFSIZE);
		sprintf(buf, "%s.1", filename);
		name_size = strlen(buf);
		
		if (conns[0] != -1) {
			write(conns[0], buf, BUFSIZE);
			write(conns[0], (char*)&piece, sizeof(piece));
		}
		if (conns[3] != -1) {
			write(conns[3], buf, BUFSIZE);
			write(conns[3], (char*)&piece, sizeof(piece));
		}

		while (bufs_to_send > 1) {
			bzero(buf, BUFSIZE);
			bytes_read += fread(buf, 1, BUFSIZE, fp);
			if (conns[0] != -1) {
				write(conns[0], buf, strlen(buf));
			}
			if (conns[3] != -1) {
				write(conns[3], buf, strlen(buf));
			}
			bufs_to_send -= 1;
		}

		int last_num_bytes = piece - bytes_read;
		bzero(buf, BUFSIZE);
		fread(buf, 1, last_num_bytes, fp);
		if (conns[0] != -1) {
			write(conns[0], buf, last_num_bytes);
		}
		if (conns[3] != -1) {
			write(conns[3], buf, last_num_bytes);
		}

		// send part 2 to conns 0 and 1
		//************************************************************************//
		bytes_read = 0;
		last_num_bytes = 0;
		bufs_to_send = piece / (float)BUFSIZE;
		bzero(buf, BUFSIZE);
		sprintf(buf, "%s.2", filename);

		if (conns[0] != -1) {
			write(conns[0], buf, BUFSIZE);
			write(conns[0], (char*)&piece, sizeof(piece));
		}
		if (conns[1] != -1) {
			write(conns[1], buf, BUFSIZE);
			write(conns[1], (char*)&piece, sizeof(piece));
		}

		while (bufs_to_send > 1) {
			bzero(buf, BUFSIZE);
			bytes_read += fread(buf, 1, BUFSIZE, fp);
			if (conns[0] != -1) {
				write(conns[0], buf, strlen(buf));
			}
			if (conns[1] != -1) {
				write(conns[1], buf, strlen(buf));
			}
			bufs_to_send -= 1;
		}

		last_num_bytes = piece - bytes_read;
		bzero(buf, BUFSIZE);
		fread(buf, 1, last_num_bytes, fp);
		if (conns[0] != -1) {
			write(conns[0], buf, last_num_bytes);
		}
		if (conns[1] != -1) {
			write(conns[1], buf, last_num_bytes);
		}

		// // send part 3 to conns 1 and 2
		//************************************************************************//
		bytes_read = 0;
		last_num_bytes = 0;
		bufs_to_send = piece / (float)BUFSIZE;
		bzero(buf, BUFSIZE);
		sprintf(buf, "%s.3", filename);

		if (conns[2] != -1) {
			write(conns[2], buf, BUFSIZE);
			write(conns[2], (char*)&piece, sizeof(piece));
		}
		if (conns[1] != -1) {
			write(conns[1], buf, BUFSIZE);
			write(conns[1], (char*)&piece, sizeof(piece));
		}

		while (bufs_to_send > 1) {
			bzero(buf, BUFSIZE);
			bytes_read += fread(buf, 1, BUFSIZE, fp);
			if (conns[2] != -1) {
				write(conns[2], buf, strlen(buf));
			}
			if (conns[1] != -1) {
				write(conns[1], buf, strlen(buf));
			}
			bufs_to_send -= 1;
		}

		last_num_bytes = piece - bytes_read;
		bzero(buf, BUFSIZE);
		fread(buf, 1, last_num_bytes, fp);
		if (conns[2] != -1) {
			write(conns[2], buf, last_num_bytes);
		}
		if (conns[1] != -1) {
			write(conns[1], buf, last_num_bytes);
		}

		// // send part 4 to conns 2 and 3
		//************************************************************************//
		piece = piece + extra_bytes;
		bytes_read = 0;
		last_num_bytes = 0;
		bufs_to_send = piece / (float)BUFSIZE;
		bzero(buf, BUFSIZE);
		sprintf(buf, "%s.4", filename);

		if (conns[2] != -1) {
			write(conns[2], buf, BUFSIZE);
			write(conns[2], (char*)&piece, sizeof(piece));
		}
		if (conns[3] != -1) {
			write(conns[3], buf, BUFSIZE);
			write(conns[3], (char*)&piece, sizeof(piece));
		}

		while (bufs_to_send > 1) {
			bzero(buf, BUFSIZE);
			bytes_read += fread(buf, 1, BUFSIZE, fp);
			if (conns[2] != -1) {
				write(conns[2], buf, strlen(buf));
			}
			if (conns[3] != -1) {
				write(conns[3], buf, strlen(buf));
			}
			bufs_to_send -= 1;
		}

		last_num_bytes = piece - bytes_read;
		bzero(buf, BUFSIZE);
		fread(buf, 1, last_num_bytes, fp);
		if (conns[2] != -1) {
			write(conns[2], buf, last_num_bytes);
		}
		if (conns[3] != -1) {
			write(conns[3], buf, last_num_bytes);
		}

		fclose(fp);
	}
	else {
		printf("Error opening file in put.\n");
	}
}




/*
 * md5sum hash function
 */
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