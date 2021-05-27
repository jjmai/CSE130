#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#define __USE_XOPEN
#include <time.h>

#define buffer_size 1024

typedef struct cache {
  char data[buffer_size];
  int size;
  int length;
  int max;
  int fsize;
  int u;
  char tag[buffer_size];
  struct cache *next;
  time_t time;
} cache;

cache *c = NULL;
/**
   Creates a socket for connecting to a server running on the same
   computer, listening on the specified port number.  Returns the
   socket file descriptor on succes.  On failure, returns -1 and sets
   errno appropriately.
 */
int create_client_socket(uint16_t port) {
  int clientfd = socket(AF_INET, SOCK_STREAM, 0);
  if (clientfd < 0) {
    return -1;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (connect(clientfd, (struct sockaddr *)&addr, sizeof addr)) {
    return -1;
  }
  return clientfd;
}

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
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(listenfd, (struct sockaddr *)&addr, sizeof addr) < 0) {
    err(EXIT_FAILURE, "bind error");
  }

  if (listen(listenfd, 500) < 0) {
    err(EXIT_FAILURE, "listen error");
  }

  return listenfd;
}

cache *check_cache(char *tag) {
  cache *ptr = c;
  while (ptr != NULL) {
    if (strcmp(ptr->tag, tag) == 0) {
      return ptr;
    }
    ptr = ptr->next;
  }
  return NULL;
}

void write_cache(char *tag, char *response, int length, time_t time) {
  if (length > c->fsize)
    return;
  // cache is not full
  if (c->size < c->max) {

    cache *node = malloc(sizeof(cache));
    strcpy(node->data, response);
    strcpy(node->tag, tag);
    node->next = c;
    node->time = time;
    c = node;
    c->length = length;
    c->size++;
  } else if (c->size == c->max) {
    int lru = 1000000;
    cache *ptr = NULL;
    cache *least = NULL;
    ptr = c;
    if (c->u == 'L') {
      while (ptr != NULL) {
        if (ptr->time < lru) {
          lru = ptr->time;
          least = ptr;
        }
        ptr = ptr->next;
      }
      strcpy(least->data, response);
      strcpy(least->tag, tag);
      least->time = time;
      least->length = length;
    } else if (c->u == 'F') {
      while (ptr != NULL) {
        if (ptr->next == NULL) {
          least = ptr;
        }
        ptr = ptr->next;
      }
      strcpy(least->data, response);
      strcpy(least->tag, tag);
      least->time = time;
      least->length = length;
    }
  }
  return;
}

void read_cache(cache *temp, char *resp, time_t ret) {
  // if newer than stored
  if (ret >= temp->time) {
    strcpy(resp, temp->data);
    temp->time = ret;
  }
}

void handle_get(int connfd, int serverfd, char *buffer) {
  struct stat fs;
  char copy[buffer_size];
  char request[buffer_size];
  char uri[buffer_size];
  char version[buffer_size];
  char host_name[buffer_size];
  char host[buffer_size];
  char resp[buffer_size];
  int valread = 0;
  struct tm times;
  char *r;
  int n = 0;
  time_t ret;

  sscanf(buffer, "%s %s %s %s %s", request, uri, version, host_name, host);

  cache *temp = NULL;
  if ((temp = check_cache(uri)) != NULL) {
    sprintf(copy, "HEAD %s %s\r\n%s %s\r\n\r\n", uri, version, host_name, host);
    valread = send(serverfd, copy, strlen(copy), 0);
    valread = recv(serverfd, resp, buffer_size, 0);
    r = strstr(resp, "Last-Modified:");
    strptime(r, "Last-Modified: %a, %d %b %Y %T GMT ", &times);
    // n = strftime(r, buffer_size, "%a, %d %b %Y %T", &times);
    ret = mktime(&times);
    // compare new times
    if (ret > temp->time) {
      read_cache(temp, resp, ret);
      // printf("%s\n",resp);
    } else {
      send(connfd, temp->data, temp->length, 0);
    }

  } else {

    valread = send(serverfd, buffer, strlen(buffer), 0);
    valread = recv(serverfd, resp, buffer_size, 0);

    r = strstr(resp, "Last-Modified:");
    strptime(r, "Last-Modified: %a, %d %b %Y %T GMT ", &times);
    // n = strftime(r, buffer_size, "%a, %d %b %Y %T", &times);
    ret = mktime(&times);

    write_cache(uri, resp, valread, ret);
    send(connfd, resp, valread, 0);
  }

  return;
}

void handle_put(int connfd, int serverfd, char *buffer) {

  int fd = 0, n = 0, valread = 0;
  char copy[buffer_size];

  while (1) {
    n = recv(connfd, copy, buffer_size, 0);
  }

  n = send(serverfd, buffer, strlen(buffer), 0);
  if (n > 0) {
    n = recv(serverfd, copy, strlen(copy), 0);
    printf("%s\n", copy);
  }
}

void handle_connection(int connfd, int serverfd) {
  // do something
  char buffer[buffer_size];
  char request[buffer_size];
  char body[buffer_size];
  char version[buffer_size];
  char content[buffer_size];
  char content_num[buffer_size];
  char copy[buffer_size];
  char host_name[buffer_size];
  char host[buffer_size];
  int valread = 0;
  char *p;

  while ((valread = recv(connfd, buffer, buffer_size, 0)) > 0) {
    write(STDOUT_FILENO, buffer, valread);

    sscanf(buffer, "%s %s %s %s %s", request, body, version, host_name, host);

    if (strcmp(request, "GET") == 0) {
      handle_get(connfd, serverfd, buffer);
    } else if (strcmp(request, "PUT") == 0) {
      p = strstr(buffer, "Content");
      sscanf(p, "%s %s", content, content_num);
      handle_put(connfd, serverfd, buffer);
    }
  }
  // when done, close socket
  close(connfd);
  close(serverfd);
}

int main(int argc, char *argv[]) {
  int listenfd, serverfd, servernum;
  uint16_t port;
  int opt;
  int counter = 0, c_size = 3, m_size = 65536, u_size = 'F';

  // You will have to modify this and add your own argument parsing
  if (argc < 2) {
    errx(EXIT_FAILURE, "wrong arguments: %s port_num", argv[0]);
  }

  int i = 0;
  while (optind < argc) {
    i++;
    if ((opt = getopt(argc, argv, "c:m:u")) != -1) {
      switch (opt) {
      case 'c':
        c_size = atoi(optarg);
        if (c_size <= 0) {
          errx(EXIT_FAILURE, "invalid size of cashe input\n");
        }
        break;
      case 'm':
        m_size = atoi(optarg);
        break;
      case 'u':
        u_size = 'L';
        break;
      case '?':
        printf("WRONG FLAG\n");
        exit(1);
      default:
        break;
      }
      i++;
    } else {
      optind++;
      port = strtouint16(argv[i]);

      if (port == 0 || port < 1024) {
        errx(EXIT_FAILURE, "invalid port number: %s", argv[i]);
      }
      if (counter == 0) {
        listenfd = create_listen_socket(port);
        counter++;
      } else if (counter == 1) {
        servernum = port;
        counter++;
      }
    }
  }

  c = (cache *)malloc(sizeof(cache));
  c->size = 0;
  c->max = c_size;
  c->fsize = m_size;
  c->u = u_size;
  c->length = 0;

  while (1) {
    write(STDOUT_FILENO, "Waiting for Connection...\n", 26);
    int connfd = accept(listenfd, NULL, NULL);
    serverfd = create_client_socket(servernum);
    if (connfd < 0 || serverfd < 0) {
      warn("accept error");
      continue;
    }
    handle_connection(connfd, serverfd);
  }
  return EXIT_SUCCESS;
}
