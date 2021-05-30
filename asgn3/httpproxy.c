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
int max = 3, fsize = 65536, size = 0;
char u = 'F';

typedef struct cache {
  char *data;
  char tag[buffer_size];
  struct cache *next;
  int length;
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

void print_cache() {
  if (size > 0) {
    cache *ptr = c;
    while (ptr != NULL) {
      printf("%s-> %d\n", ptr->tag, ptr->length);
      ptr = ptr->next;
    }
  }
  return;
}

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

void write_cache(char *tag, char *response, int length, time_t time) {
  if (length > fsize)
    return;
  if (size == 0 && max == 0) {
    return;
  }

  // cache is not full
  if (size < max) {
    cache *node = malloc(sizeof(cache));
    if (node != NULL) {
      // maybe allocate more data space
      node->data = malloc(fsize);
      strcpy(node->data, response);
      strcpy(node->tag, tag);
      node->next = c;
      node->time = time;
      c = node;
      c->length = length;
      size++;
    }

  } else if (size == max) {

    cache *ptr = NULL;
    cache *least = NULL;
    ptr = c;
    time_t lru = ptr->time;
    if (u == 'L') {
      while (ptr != NULL) {
        if (ptr->time <= lru) {
          lru = ptr->time;
          least = ptr;
        }
        ptr = ptr->next;
      }
      if (least != NULL) {
        strcpy(least->data, response);
        strcpy(least->tag, tag);
        least->time = time;
        least->length = length;
      }
    } else if (u == 'F') {
      while (ptr->next != NULL) {
        ptr = ptr->next;
      }
      if (ptr != NULL) {
        strcpy(ptr->data, response);
        strcpy(ptr->tag, tag);
        ptr->time = time;
        ptr->length = length;
      }
    }
  }
  return;
}

void read_cache(cache *temp, char *resp, int length, time_t ret) {
  // if newer than stored
  if (ret > temp->time) {
    strcpy(temp->data, resp);
    temp->time = ret;
    temp->length = length;
  }
}

void handle_get(int connfd, int serverfd, char *buffer) {
  struct stat fs;
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
  char time_array[buffer_size];
  char *memory_buffer;
  memory_buffer = (char *)malloc(sizeof(char) * fsize);
  long valread = 0;
  struct tm times;
  memset(&times, 0, sizeof(struct tm));
  char *r;
  char *p;
  r = (char*)malloc(sizeof(char)* fsize);
  p = (char*)malloc(sizeof(char)* fsize);
  int n = 0, total = 0;
  time_t ret;

  sscanf(buffer, "%s %s %s %s %s", request, uri, version, host_name, host);

  cache *temp = NULL;
  if ((temp = check_cache(uri)) != NULL) {
    sprintf(copy, "HEAD %s %s\r\n%s %s\r\n\r\n", uri, version, host_name, host);
    valread = send(serverfd, copy, strlen(copy), 0);
    valread = recv(serverfd, resp, fsize, 0);
    
    r = strstr(resp, "Last-Modified:");
    strptime(r, "Last-Modified: %a, %d %b %Y %T GMT ", &times);
    // n = strftime(r, buffer_size, "%a, %d %b %Y %T", &times);
    ret = mktime(&times);
    // compare new times
  
    if (ret > temp->time) {
      send(serverfd, buffer, strlen(buffer), 0);
      valread = recv(serverfd, resp, fsize, 0);
      // write_cache(uri,resp,valread,ret);
      read_cache(temp, resp, valread, ret);
      send(connfd, resp, valread, 0);
    } else {
      
     int nn =  send(connfd, temp->data, temp->length, 0); 
     
    }

  } else {
    // send HEAD?
    sprintf(copy, "HEAD %s %s\r\n%s %s\r\n\r\n", uri, version, host_name, host);
    valread = send(serverfd, copy, strlen(copy), 0);
    valread = recv(serverfd, resp, fsize, 0);
    sscanf(resp, "%s %s", version, code);
    // file dont exist
    if (strcmp(code, "404") == 0) {
      sprintf(copy, "%s 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n",
              version);
      send(connfd, copy, strlen(copy), 0);

    } else {
      p = strstr(resp, "Content");
      if (p == NULL) {
        printf("Error, content don't exist\n");
        exit(1);
      }
      sscanf(p, "%s %s", content, content_num);
      long cont_num = atoi(content_num);

      send(serverfd, buffer, strlen(buffer), 0);

      while (total < cont_num) {
        valread = recv(serverfd, copy, fsize, 0);
         memcpy(memory_buffer + total, copy, valread);
        n = send(connfd, copy, valread, 0);
        total += valread;
      }

      r = strstr(resp, "Last-Modified:");
      if(r== NULL) {
        printf("Error getting date\n");
	exit(1);
      }
    
      strptime(r, "Last-Modified: %a, %d %b %Y %T GMT ", &times);

      //    n = strftime(r, buffer_size, "%a, %d %b %Y %T", &times);
      ret = mktime(&times);
      write_cache(uri, memory_buffer, total, ret);
      // n = send(connfd, memory_buffer, total, 0);
      // write(STDOUT_FILENO,resp,n);
    }
  }
  print_cache();
  return;
}

void handle_put(int connfd, int serverfd, char *buffer) {

  int n = 0, valread = 0, total = 0;
  char copy[buffer_size];
  char content[fsize];
  char content_num[fsize];
  char *memory_buffer;
  char *p;
  memory_buffer = (char *)malloc(sizeof(char) * fsize);

  // parse
  valread = send(serverfd, buffer, strlen(buffer), 0);

  p = strstr(buffer, "Content");
  sscanf(p, "%s %s", content, content_num);
  while (total < atoi(content_num)) {
    valread = recv(connfd, copy, buffer_size, 0);
    total += valread;
    send(serverfd, copy, valread, 0);
  }

  if (total > 0) {
    n = recv(serverfd, copy, buffer_size, 0);
    n = send(connfd, copy, n, 0);
  }
}

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
  int listenfd, serverfd, servernum;
  uint16_t port;
  int opt;
  int counter = 0;

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

  c = NULL;

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
