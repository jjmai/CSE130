#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
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

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;

#define buffer_size 32768

// process a GET request and response
// builds a response to be sent over to client
void send_get(int connfd, char *body, char *version) {
  char *copy;
  char *read_buffer;
  int fd = 0;
  int read_len = 0, r = 0;
  struct stat fs;
  copy = (char *)calloc(buffer_size, sizeof(char));
  read_buffer = (char *)calloc(buffer_size, sizeof(char));

  // deletes the / in front
  memmove(&body[0], &body[1], strlen(body));

  fd = open(body, O_RDONLY);

  // if file doesn't exists
  if (fd < 0) {
    sprintf(copy, "%s 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n",
            version);
    send(connfd, copy, strlen(copy), 0);
    close(connfd);
  } else {
    r = stat(body, &fs);
    // no permission on file
    if (r == -1) {
      sprintf(copy, "%s 403 Forbidden\r\nContent-Length:10\r\n\r\nForbidden\n",
              version);
      send(connfd, copy, strlen(copy), 0);
      close(connfd);
    } else {
      int fsize = fs.st_size;
      // realloc more space in buffer if needed
      if (fsize > buffer_size) {
        read_buffer = (char *)realloc(read_buffer, fsize);
        copy = (char *)realloc(copy, fsize);
      }
      read_len = read(fd, read_buffer, fsize);
      read_buffer[read_len] = '\0';

      // if able to read byte>0
      if (read_len > 0) {
        sprintf(copy, "%s 200 OK\r\nContent-Length: %d\r\n\r\n", version,
                read_len);
        // pthread_mutex_unlock(&lock);
        // sleep(1);
        send(connfd, copy, strlen(copy), 0);
        send(connfd, read_buffer, read_len, 0);
      } else {
        // 0 size byte
        sprintf(copy,
                "%s 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n",
                version);
        send(connfd, copy, strlen(copy), 0);
      }
    }
  }
  close(fd);
}

// handles PUT requese and response
// builds a response header
void send_put(int connfd, char *body, char *version, char *content_num) {
  char *copy;
  char *read_buffer;
  int fd = 0;
  int write_len = 0, valread = 0, r = 0;
  struct stat fs;
  copy = (char *)calloc(atoi(content_num), sizeof(char));
  read_buffer = (char *)calloc(atoi(content_num), sizeof(char));

  memmove(&body[0], &body[1], strlen(body));
  fd = access(body, F_OK);
  // if file doesn't exist, we create one
  if (fd < 0) {
    fd = open(body, O_CREAT | O_WRONLY | O_TRUNC);

    valread = recv(connfd, read_buffer, atoi(content_num), 0);
    write_len = write(fd, read_buffer, valread);
    // if able to write to a created file
    sprintf(copy, "%s 201 Created\r\nContent-Length: %d\r\n\r\n", version,
            write_len);
    send(connfd, copy, strlen(copy), 0);
    send(connfd, read_buffer, write_len, 0);
  } else {
    r = stat(body, &fs);
    // if no permission on file
    if (r == -1) {
      sprintf(copy, "%s 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n",
              version);
      send(connfd, copy, strlen(copy), 0);
      close(connfd);
    } else {
      fd = open(body, O_CREAT | O_WRONLY | O_TRUNC);
      valread = recv(connfd, read_buffer, atoi(content_num), 0);
      write_len = write(fd, read_buffer, valread);
      // if able to write to existing file
      if (write_len > 0) {
        sprintf(copy, "%s 200 OK\r\nContent-Length: %d\r\n\r\n", version,
                write_len);
        send(connfd, copy, strlen(copy), 0);
        send(connfd, read_buffer, write_len, 0);
      } else {
        // if something faile don program
        sprintf(copy,
                "%s 500 Internal Server Error\r\nContent-Length: "
                "22\r\n\r\nInternal Server Error\n",
                version);
        send(connfd, copy, strlen(copy), 0);
      }
    }
  }
  close(fd);
}
// responsible for handling HEAD request and response
void send_head(int connfd, char *body, char *version) {
  char *read_buffer;
  char *copy;
  copy = (char *)calloc(buffer_size, sizeof(char));
  read_buffer = (char *)calloc(buffer_size, sizeof(char));
  int valread = 0, r = 0, infile = 0;
  struct stat fs;

  memmove(&body[0], &body[1], strlen(body));
  infile = open(body, O_RDONLY, 0);
  if (infile < 0) {
    r = stat(body, &fs);
    if (r == -1) {
      sprintf(copy, "%s 403 Forbidden\r\nContent-Length: 10\r\n\r\n", version);
      send(connfd, copy, strlen(copy), 0);
    } else {
      sprintf(copy, "%s 404 Not Found\r\nContent-Length: 10\r\n\r\n", version);
      send(connfd, copy, strlen(copy), 0);
    }
  } else if (infile > 0) {
    int fsize = fs.st_size;
    if (fsize > buffer_size) {
      read_buffer = (char *)realloc(read_buffer, fsize);
    }
    valread = read(infile, read_buffer, buffer_size);
    sprintf(copy, "%s 200 OK\r\nContent-Length: %d\r\n\r\n", version, valread);
    send(connfd, copy, strlen(copy), 0);
  } else {
    sprintf(copy,
            "%s 500 Internal Server Error\r\nContent-Length: "
            "22\r\n\r\n",
            version);
    send(connfd, copy, strlen(copy), 0);
  }

  close(infile);
}

// check if file is ASCII values only
int checker(char *body) {
  for (int i = 1; i < 16; i++) {
    if (isalpha(body[i]) == 0 && isdigit(body[i]) == 0) {
      return 0;
    }
  }
  return 1;
}

// data structure

typedef struct stack {
  int count;
  int size;
  int front;
  int back;
  int shared_integer;
  int *shared_buffer;
} stack;

stack *s;

// char buff[buffer_size];
// int integer;
int counter = 0;
void *handle_connection(void *arg) {
  // do something
  int *conn_fd_pointer = (int *)arg;
  int connfd = *conn_fd_pointer;
  int valread = 0;
  char buffer[buffer_size];
  char request[buffer_size];
  char body[buffer_size];
  char version[buffer_size];
  char content[buffer_size];
  char content_num[buffer_size];
  char copy[buffer_size];
  char *p;

  while ((valread = recv(connfd, buffer, buffer_size, 0)) > 0) {
    // lock variable?
  //  pthread_mutex_lock(&lock);
    counter++;
    printf("Job %d has started \n", counter);

    write(STDOUT_FILENO, buffer, valread);

    sscanf(buffer, "%s %s %s ", request, body, version);
    // checks to see if request is good
    if (strstr(version, "HTTP") == NULL || strlen(body) != 16) {
      sprintf(copy,
              "%s 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n",
              version);
      send(connfd, copy, strlen(copy), 0);
      break;
    }
    // checks if file is ascii
    if (checker(body) == 0) {
      sprintf(copy,
              "%s 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n",
              version);
      send(connfd, copy, strlen(copy), 0);
      break;
    }

    if (strcmp(request, "GET") == 0) {
      send_get(connfd, body, version);
    } else if (strcmp(request, "PUT") == 0) {
      p = strstr(buffer, "Content");
      if (p == NULL) {
        sprintf(copy,
                "%s 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n",
                version);
        send(connfd, copy, strlen(copy), 0);
      }
      sscanf(p, "%s %s", content, content_num);
      send_put(connfd, body, version, content_num);

    } else if (strcmp(request, "HEAD") == 0) {
      send_head(connfd, body, version);
    } else {
      sprintf(copy,
              "%s 501 Not Implemented\r\nContent-Length:16\r\n\r\nNot "
              "Implemented\n",
              version);
      send(connfd, copy, strlen(copy), 0);
    }
    printf("Job %d has finished \n", counter);
   // pthread_mutex_unlock(&lock);
    sleep(1);
  }

  // when done, close socket
  close(connfd);
  return NULL;
}

// use shared buffer data structure
void *handle_thread(void *arg) {

  while (1) {

    pthread_mutex_lock(&lock);
    // dequeue
    // pthread_cond_wait(&cond1,&lock);
    while (s->size == 0) {
      pthread_cond_wait(&cond1, &lock);
    }
    int connfd = s->shared_buffer[s->front];
    s->size--;
    s->front++;    
    pthread_mutex_unlock(&lock);

    handle_connection(&connfd);
  }
  return NULL;
}

void *thread_add(int connfd) {

  pthread_mutex_lock(&lock);
  // push to stack
  s->size++;
  s->shared_buffer[s->back] = connfd;
  s->back++;

  pthread_mutex_unlock(&lock);
  pthread_cond_signal(&cond1);

  return NULL;
}

int main(int argc, char *argv[]) {
  int listenfd;
  uint16_t port;
  int opt = 0;

  if (argc != 2) {
    errx(EXIT_FAILURE, "wrong arguments: %s port_num", argv[0]);
  }
  port = strtouint16(argv[1]);
  if (port == 0) {
    errx(EXIT_FAILURE, "invalid port number: %s", argv[1]);
  }
  listenfd = create_listen_socket(port);
  // get opt here
  int n = 4, i = 0;
  while ((opt = getopt(argc, argv, "nl:")) != -1) {
    switch (opt) {
    case 'n':
      break;
    case 'l':
      break;
    default:
      break;
    }
  }
  pthread_t worker[n], dispatcher[n];
  // pthread_mutex_init(&lock,NULL);

  s = (stack *)malloc(sizeof(stack));
  s->size = 0;
  s->front = 0;
  s->back = 0;
  s->shared_buffer = (int *)malloc(sizeof(int *) * buffer_size);

  for (int j = 0; j < n; j++) {
    pthread_create(&(dispatcher[j]), NULL, handle_thread, (void *)NULL);
  }
  while (1) {
    printf("Waiting for connection...\n");
    int connfd = accept(listenfd, NULL, NULL);
    if (connfd < 0) {
      warn("accept error");
      continue;
    }
    thread_add(connfd);
  }
  return EXIT_SUCCESS;
}
