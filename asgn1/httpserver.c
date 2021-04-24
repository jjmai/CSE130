#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <err.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
/**
   Converts a string to an 16 bits unsigned integer.
   Returns 0 if the string is malformed or out of the range.
 */
uint16_t strtouint16(char number[]) {
  char *last;
  long num = strtol(number, &last, 10);
  if (num <= 0 || num > UINT16_MAX || *last != '\0') {
    return 0;
  }
  return num;
}

/**
   Creates a socket for listening for connections.
   Closes the program and prints an error message on error.
 */
int create_listen_socket(uint16_t port) {
  struct sockaddr_in addr;
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    err(EXIT_FAILURE, "socket error");
  }

  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htons(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(listenfd, (struct sockaddr *)&addr, sizeof addr) < 0) {
    err(EXIT_FAILURE, "bind error");
  }

  if (listen(listenfd, 500) < 0) {
    err(EXIT_FAILURE, "listen error");
  }

  return listenfd;
}

#define buffer_size 1024
void send_get(int connfd, char *request, char *body, char *version) {
  char copy[200];
  char read_buffer[200];
  int fd = STDIN_FILENO;
  int read_len = 0,r=0;
  struct stat fs;

  memmove(&body[0], &body[1], strlen(body));
  r = stat (body,&fs);
  //no permission
  if( r== -1) {
    sprintf(copy,"%s 403 Forbidden\r\n",version);
    send(connfd,copy,strlen(copy),0);
  }

  fd = open(body, O_RDONLY);

  //create new file 201
  if (fd < 0) {
    // creat new file
    fd = open(body, O_CREAT | O_RDWR | O_TRUNC);
  } else {
    read_len = read(fd, read_buffer, buffer_size);
    read_buffer[read_len] = '\0';
    if (read_len > 0) {
      sprintf(copy, "%s 200 OK\r\nContent-Length: %d\r\n\r\n%s\n", version,
              read_len, read_buffer);
      send(connfd, copy, strlen(copy), 0);
    }
  }
}

// (socket,request,file,version, bytes,code)
void send_put(int connfd, char *request, char *body, char *version,
              char *content, char *content_num) {
  char copy[1024];
  char read_buffer[1024];
  int fd = STDIN_FILENO;
  int write_len = 0, valread = 0;

  memmove(&body[0], &body[1], strlen(body));
  fd = open(body, O_WRONLY);
  if (fd < 0) {
    fd = open(body, O_CREAT | O_WRONLY | O_TRUNC);
    // write_len = write(fd, read_buffer, atoi(content_num));

    valread = recv(connfd, read_buffer, buffer_size, 0);
    // send(connfd,code,strlen(code),0);
    write_len = write(fd, read_buffer, valread);
    sprintf(copy, "%s 201 Created\r\n%s %d\r\n\r\n%s\n", version, content,
            write_len, read_buffer);
    send(connfd, copy, strlen(copy), 0);

  } else {
    valread = recv(connfd, read_buffer, buffer_size, 0);
    write_len = write(fd, read_buffer, valread);
    sprintf(copy, "%s 200 OK\r\n%s %d\r\n\r\n%s\n", version, content,
            write_len, read_buffer);
    send(connfd, copy, strlen(copy), 0);
  }
}

void send_head(int connfd, char *request, char *body, char *version) {
  char read_buffer[1024];
  int infile = STDIN_FILENO;
  char copy[200];
  int valread = 0;

  memmove(&body[0], &body[1], strlen(body));
  infile = open(body, O_RDONLY);
  if (infile > 0) {
    valread = read(infile, read_buffer, buffer_size);
    sprintf(copy, "%s 200 OK\r\nContent-Length %d\r\n\r\n", version, valread);
    send(connfd, copy, strlen(copy), 0);
  }
  close(infile);
}

void handle_connection(int connfd) {
  // do something
  int infile = STDIN_FILENO;
  int outfile = STDOUT_FILENO;
  int valread = 0;
  //  char read_buffer[buffer_size];
  char buffer[buffer_size];
  char request[20];
  char body[200];
  char version[200];
  char content[200];
  char content_num[200];
  //  char copy[2000];
  // int fd = 0;
  char code[] = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n";
  char *p;
  // int write_len = 0, read_len = 0;

  while ((valread = recv(connfd, buffer, buffer_size, 0)) > 0) {
    write(outfile, buffer, valread);
    sscanf(buffer, "%s %s %s ", request, body, version);

    if (strcmp(request, "GET") == 0) {
      send_get(connfd, request, body, version);
      // send(connfd, code, strlen(code), 0);

    } else if (strcmp(request, "PUT") == 0) {
      p = strstr(buffer, "Content");
      sscanf(p, "%s %s", content, content_num);

      send_put(connfd, request, body, version, content, content_num);
      // send(connfd, code, strlen(code), 0);
    } else if (strcmp(request, "HEAD") == 0) {
      send_head(connfd, request, body, version);
    }
  }
  printf("Waiting...\n");

  // when done, close socket
  close(connfd);
}

int main(int argc, char *argv[]) {
  int listenfd;
  uint16_t port;

  if (argc != 2) {
    errx(EXIT_FAILURE, "wrong arguments: %s port_num", argv[0]);
  }
  port = strtouint16(argv[1]);
  if (port == 0) {
    errx(EXIT_FAILURE, "invalid port number: %s", argv[1]);
  }
  listenfd = create_listen_socket(port);

  while (1) {
    int connfd = accept(listenfd, NULL, NULL);
    if (connfd < 0) {
      warn("accept error");
      continue;
    }
    handle_connection(connfd);
  }
  return EXIT_SUCCESS;
}
