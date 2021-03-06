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
int max = 3, fsize = 65536, size = 0, pos = 0, lru_time = 0;
char u = 'F';

// data structure for cache
typedef struct cache {
  char *data;
  char request[buffer_size];
  char tag[buffer_size];
  struct cache *next;
  long length;
  time_t time;
  int lru;
  int position;
} cache;

// global cache
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

// printing cache
void print_cache() {
  if (size > 0) {
    cache *ptr = c;
    printf("Cache Contains\n -------------------\n");

    while (ptr != NULL) {
      printf("uri: %s\nLength: %ld, lru: %d\n\n", ptr->tag, ptr->length,
             ptr->lru);
      ptr = ptr->next;
    }
    free(ptr);
  }
  return;
}

// deletes node when full
void deleteNode(cache **head, char *target) {
  cache *temp = *head, *prev = NULL;

  if (temp != NULL && strcmp(temp->tag, target) == 0) {
    *head = temp->next;
    free(temp);
    size--;
    pos--;
    return;
  }

  while (temp != NULL && strcmp(temp->tag, target) != 0) {
    prev = temp;
    temp = temp->next;
  }
  if (temp == NULL) {
    return;
  }
  prev->next = temp->next;
  free(temp);
  size--;
  pos--;
  return;
}

// checks if a node with same uri exists
// returns ptr to node
cache *check_cache(char *tag) {
  if (c == NULL) {
    return NULL;
  }
  cache *ptr = c;
  while (ptr != NULL) {
    if (strcmp(ptr->tag, tag) == 0) {
      return ptr;
    }
    ptr = ptr->next;
  }
  return NULL;
}

// write to cache
void write_cache(char *tag, char *request, char *response, long length,
                 time_t time) {
  if (length > fsize)
    return;
  if (size == 0 && max == 0) {
    return;
  }

  // cache is not full
  if (size < max) {
    // create a new node
    cache *node = malloc(sizeof(cache));

    node->position = 0;
    node->next = NULL;
    node->time = 0;
    node->lru = 0;
    node->length = 0;
    if (node != NULL) {
      node->data = malloc(fsize);
      memset(node->data, 0, fsize);
      strcpy(node->request, request);
      memcpy(node->data, response, length);
      strcpy(node->tag, tag);
      node->next = c;
      node->time = time;
      node->position = pos++;
      node->length = length;
      node->lru = lru_time;
      c = node;
      size++;
    }
    // full cache
  } else if (size == max) {

    cache *ptr = NULL;
    cache *least = NULL;
    ptr = c;
    // for LRU
    if (u == 'L') {
      int lru = ptr->lru;
      while (ptr != NULL) {
        if (ptr->lru <= lru) {
          lru = ptr->lru;
          least = ptr;
        }
        ptr = ptr->next;
      }
      if (least != NULL) {
        strcpy(least->request, request);
        memcpy(least->data, response, length);
        strcpy(least->tag, tag);
        least->time = time;
        least->length = length;
        least->lru = lru_time;
      }
      // for FIFO
    } else if (u == 'F') {
      while (ptr != NULL) {
        if (ptr->position == 0) {
          ptr->position = max - 1;
          least = ptr;
        } else {
          ptr->position--;
        }
        ptr = ptr->next;
      }
      if (least != NULL) {
        strcpy(least->request, request);
        memcpy(least->data, response, length);
        strcpy(least->tag, tag);
        least->time = time;
        least->length = length;
      }
    }
  }
  return;
}

// copy and replace with same URI
void read_cache(cache *temp, char *request, char *resp, long length,
                time_t ret) {
  if (length > fsize || max == 0) {
    return;
  }
  // only replace if time modified
  // is newer
  if (ret > temp->time) {
    strcpy(temp->request, request);
    memcpy(temp->data, resp, length);
    temp->time = ret;
    temp->length = length;
  }
}

// free
void free_cache() {
  while (c != NULL) {
    free(c->data);
  }
  free(c);
}

// for 400 and 404 error recv/send
int error_check(int connfd, int serverfd, char *code, int cont_num) {
  // file dont exist
  char copy[buffer_size];
  int n = 0, total = 0;
  if (strcmp(code, "404") == 0) {
    while (total < cont_num) {
      n = recv(serverfd, copy, buffer_size, 0);
      send(connfd, copy, n, 0);
      total += n;
    }
    return 0;

  } else if (strcmp(code, "400") == 0) {
    while (total < cont_num) {
      n = recv(serverfd, copy, buffer_size, 0);
      send(connfd, copy, n, 0);
      total += n;
    }
    return 0;
  }
  return 1;
}

// for GET requests
void handle_get(int connfd, int serverfd, char *buffer) {
  char copy[fsize];
  char request[buffer_size];
  char uri[buffer_size];
  char version[buffer_size];
  char host_name[buffer_size];
  char host[buffer_size];
  char content[buffer_size];
  char content_num[buffer_size];
  char resp[fsize];
  char code[buffer_size];
  char *memory_buffer;
  long valread = 0, total = 0, cont_num = 0;
  struct tm times;
  memset(&times, 0, sizeof(struct tm));
  char *r;
  char *p;
  r = (char *)malloc(sizeof(char) * fsize);
  p = (char *)malloc(sizeof(char) * fsize);
  int n = 0;
  time_t ret;
  int q = 0;
  memory_buffer = (char *)malloc(sizeof(char) * fsize);
  memset(memory_buffer, 0, sizeof(char) * fsize);
  sscanf(buffer, "%s %s %s %s %s", request, uri, version, host_name, host);

  lru_time++;
  cache *temp = NULL;
  // if exists in cache
  if ((temp = check_cache(uri)) != NULL) {
    // send head to retrieve Date
    sprintf(copy, "HEAD %s %s\r\n%s %s\r\n\r\n", uri, version, host_name, host);
    valread = send(serverfd, copy, strlen(copy), 0);
    valread = recv(serverfd, resp, fsize, 0);

    r = strstr(resp, "Last-Modified:");
    if (r == NULL) {
      close(connfd);
      errx(EXIT_FAILURE, "Failed to get date\n");
    }
    strptime(r, "Last-Modified: %a, %d %b %Y %T GMT ", &times);
    ret = mktime(&times);
    // if new modified time
    if (ret > temp->time) {
      send(serverfd, buffer, strlen(buffer), 0);
      // receives only header
      while (1) {
        valread = recv(serverfd, memory_buffer + q, 1, 0);
        q += valread;
        p = strstr(memory_buffer, "\r\n\r\n");
        if (p != NULL) {
          send(connfd, memory_buffer, q, 0);
          break;
        }
      }
      p = strstr(memory_buffer, "Content");
      if (p == NULL) {
        close(connfd);
        errx(EXIT_FAILURE, "Failure on getting Content\n");
      }
      sscanf(p, "%s %s", content, content_num);
      cont_num = atoi(content_num);

      sscanf(memory_buffer, "%s %s", version, code);
      if (error_check(connfd, serverfd, code, cont_num) == 1) {
        // receives all content
        while (total < cont_num) {
          valread = recv(serverfd, copy, fsize, 0);
          n = send(connfd, copy, valread, 0);
          total += valread;
        }
        // only cache if less than fsize
        if (total < fsize) {
          read_cache(temp, memory_buffer, copy, total, ret);
          temp->lru = lru_time;
        }
      }
      // no new time
    } else {
      temp->lru = lru_time;
      send(connfd, temp->request, strlen(temp->request), 0);
      send(connfd, temp->data, temp->length, 0);
    }
    // not in cache
  } else {
    send(serverfd, buffer, strlen(buffer), 0);
    // receives header only
    while (1) {
      valread = recv(serverfd, memory_buffer + q, 1, 0);
      q += valread;
      p = strstr(memory_buffer, "\r\n\r\n");
      if (p != NULL) {
        send(connfd, memory_buffer, q, 0);
        break;
      }
    }
    p = strstr(memory_buffer, "Content");
    if (p == NULL) {
      close(connfd);
      errx(EXIT_FAILURE, "Failed on retrieving COntent\n");
    }
    sscanf(p, "%s %s", content, content_num);
    cont_num = atoi(content_num);

    sscanf(memory_buffer, "%s %s", version, code);
    if (error_check(connfd, serverfd, code, cont_num) == 1) {

      r = strstr(memory_buffer, "Last-Modified:");
      if (r == NULL) {
        close(connfd);
        errx(EXIT_FAILURE, "Failed on retrieving date\n");
      }
      strptime(r, "Last-Modified: %a, %d %b %Y %T GMT ", &times);
      ret = mktime(&times);
      // receives all contents
      while (total < cont_num) {
        valread = recv(serverfd, copy, fsize, 0);
        n = send(connfd, copy, valread, 0);
        total += valread;
      }
      // only cache is less than fsize else skip
      if (total <= fsize) {
        write_cache(uri, memory_buffer, copy, total, ret);
      }
    }
  }
  // print_cache();
  return;
}

// for PUT requests
void handle_put(int connfd, int serverfd, char *buffer) {

  long n = 0, valread = 0, total = 0;
  char copy[buffer_size];
  char content[fsize];
  char content_num[fsize];
  char *memory_buffer;
  char *p;
  memory_buffer = (char *)malloc(sizeof(char) * fsize);

  valread = send(serverfd, buffer, strlen(buffer), 0);

  p = strstr(buffer, "Content");
  if (p == NULL) {
    errx(EXIT_FAILURE, "failed on content\n");
  }
  sscanf(p, "%s %s", content, content_num);
  // recv and send
  while (total < atoi(content_num)) {
    valread = recv(connfd, copy, buffer_size, 0);
    total += valread;
    send(serverfd, copy, valread, 0);
  }

  while (n < total) {
    valread = recv(serverfd, memory_buffer, fsize, 0);
    send(connfd, memory_buffer, valread, 0);
    n += valread;
  }
}

// for HEAD requests
void handle_head(int connfd, int serverfd, char *buffer) {
  int n = 0, valread = 0;
  char copy[buffer_size];

  valread = send(serverfd, buffer, strlen(buffer), 0);
  if (valread > 0) {
    n = recv(serverfd, copy, buffer_size, 0);
    n = send(connfd, copy, n, 0);
  }
}

void handle_connection(int connfd, int serverfd) {
  // do something
  char buffer[fsize];
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
    } else if (strcmp(request, "HEAD") == 0) {
      handle_head(connfd, serverfd, buffer);
    } else {
      sprintf(copy,
              "%s 501 Not Implemented\r\nContent-Length:16\r\n\r\nNot "
              "Implemented\n",
              version);
      send(connfd, copy, strlen(copy), 0);
    }
  }
  // when done, close socket
  close(connfd);
  close(serverfd);
}

int main(int argc, char *argv[]) {
  int listenfd, serverfd, servernum, clientnum;
  uint16_t port;
  int opt;
  int counter = 0;

  // You will have to modify this and add your own argument parsing
  if (argc < 2) {
    errx(EXIT_FAILURE, "wrong arguments: %s port_num", argv[0]);
  }

  while (optind < argc) {
    if ((opt = getopt(argc, argv, "c:m:u")) != -1) {
      switch (opt) {
      case 'c':
        max = atoi(optarg);
        if (max < 0) {
          errx(EXIT_FAILURE, "invalid size of cache input\n");
        }
        break;
      case 'm':
        fsize = atoi(optarg);
        if (fsize < 0) {
          errx(EXIT_FAILURE, "invalid size of file\n");
        }
        break;
      case 'u':
        u = 'L';
        break;
      case '?':
        printf("WRONG FLAG\n");
        exit(1);
      default:
        break;
      }
    } else {
      port = strtouint16(argv[optind]);
      optind++;
      if (port == 0 || port < 1024) {
        errx(EXIT_FAILURE, "invalid port number: %s", argv[optind]);
      }
      if (counter == 0) {
        clientnum = port;
        counter++;
      } else if (counter == 1) {
        servernum = port;
        counter++;
      }
    }
  }

  listenfd = create_listen_socket(clientnum);
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
  free_cache();
  return EXIT_SUCCESS;
}
