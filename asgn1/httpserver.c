#include <arpa/inet.h>
#include <err.h>
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

void send_response(int connfd, char *version) {
  dprintf(connfd, "%s 200 OK\r\n", version);
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

  while ((valread = recv(connfd, buffer, buffer_size, 0)) > 0) {
    write(outfile, buffer, valread);

    // if get received then send message
    sscanf(buffer, "%s %s %s ", request, body, version);
    if (strcmp(request, "GET") == 0) {
      // sscanf agian and p;ug in gor 200
      //  dprintf(connfd, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");

      send_response(connfd, version);
    } else if (strcmp(request, "PUT") == 0) {
      // print stuff
      // write(outfile,buffer,valread);

      dprintf(connfd, "%s 200 OK\r\n", version);
    }

    write(outfile, "waiting:\r\n", strlen("waiting\n"));
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
