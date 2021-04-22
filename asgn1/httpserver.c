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

// void get_file(int fd);

void send_response(int connfd, char *request, char *body, char *version,
                   int num) {
  if (num == 1 && (strcmp(version, "GET") == 0)) {
    dprintf(connfd, "%s 200 OK\r\n", version);
  } else if (num == 2) {
    dprintf(connfd, "%s 201 Created\r\n", version);
  } else if (num == 5) {
    dprintf(connfd, "%s 404 Not FOund\r\n", version);
  }
}

#define buffer_size 1024
void handle_connection(int connfd) {
  // do something
  int infile = STDIN_FILENO;
  int outfile = STDOUT_FILENO;
  int valread = 0;
  char read_buffer[buffer_size];
  char buffer[buffer_size];
  char request[20];
  char body[200];
  char version[200];
  char content[200];
  char copy[2000];
  int fd = 0;
  char code[] = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n";

  /**
    valread = recv(connfd, buffer, buffer_size, 0);
    write(outfile, buffer, valread);
    if (valread == 0) {
      printf("receive failed\n");
    }
    sscanf(buffer, "%s %s %s ", request, body, version);
    if (strcmp(request, "GET") == 0) {
      // open to read and store and wite

      int n = open(body, O_RDONLY);
      if (n < 0) {
        send_response(connfd, request, body, version, 5);
      }
      send_response(connfd, request, body, version, 1);
    }
  */
  while ((valread = recv(connfd, buffer, buffer_size, 0)) > 0) {
    write(outfile, buffer, valread);
    // line by line

    // if get received then send message
    sscanf(buffer, "%s %s %s ", request, body, version);

    if (strcmp(request, "GET") == 0) {
      memmove(&body[0], &body[1], strlen(body));
      fd = open(body, O_RDONLY);
      if (fd < 0) {
        printf("file don't exist,creating one \n");
        fd = open(body, O_CREAT);
        send_response(connfd, request, body, version, 2);
      } else {
        int read_len = read(fd, read_buffer, sizeof(fd) + 1);
	read_buffer[read_len]='\0';
        if (read_len > 0) {

          // send(connfd,
          sprintf(copy,
                  "%s 200 OK\r\nContent-Length: "
                  "%d\r\n\r\n%s",
                  version, read_len, read_buffer);
          printf("%s ", copy);
        }
        send(connfd, copy, strlen(copy), 0);
	send(connfd, code,strlen(code),0);
      }

     // send_response(connfd, request, body, version, 1);
    } else if (strcmp(request, "PUT") == 0) {
      // recv here
      // dprintf(connfd, "%s 200 OK\r\n", version);
      fd = open(body, O_RDONLY);
      if (fd < 0) {
        send_response(connfd, request, body, version, 5);
      }
      // write(content, body, fd);
    }

    printf("Waiting...\n");
  }
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
