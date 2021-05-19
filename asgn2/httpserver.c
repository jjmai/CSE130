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

#define buffer_size 100000
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
int log_check = 0;

// data structure

typedef struct stack {
  int count;
  int size;
  int front;
  int back;
  int capacity;
  int *shared_buffer;
} stack;

stack *s;

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

// logging into a file
int log_file;
// int log_num=0;
void logging(int num, char *request, char *body, char *host, char *version,
             int content_length) {
  // successful
  if (log_check == 1) {
    int offset = lseek(log_file, 0, SEEK_CUR);
    char nul = '\0';
    // pthread_mutex_lock(&log_lock);
    char *copy;
    copy = (char *)calloc(buffer_size, sizeof(char));
    if (num == 200 || num == 201) {
      sprintf(copy, "%s\t/%s\t%s\t%d\n", request, body, host, content_length);
      for (unsigned long i = 0; i < strlen(copy); i++) {
        write(log_file, &nul, 1);
      }

      pwrite(log_file, copy, strlen(copy), offset);
    } else {
      // fail
      sprintf(copy, "FAIL\t%s /%s %s\t%d\n", request, body, version, num);
      write(log_file, copy, strlen(copy));
    }
    // pthread_mutex_unlock(&log_lock);
  }
}

// process a GET request and response
// builds a response to be sent over to client
void send_get(int connfd, char *body, char *version, char *request,
              char *host) {
  char *copy;
  char *read_buffer;
  int fd = 0;
  long read_len = 0, r = 0;
  struct stat fs;

  copy = (char *)malloc(sizeof(char) * buffer_size);
  // read_buffer = (char *)malloc(sizeof(char) * buffer_size);

  // deletes the / in front
  memmove(&body[0], &body[1], strlen(body));
  fd = open(body, O_RDONLY, S_IRUSR | S_IWUSR);
  // r = access(body, R_OK | W_OK);

  // if file doesn't exists
  if (fd < 0) {
    sprintf(copy, "%s 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n",
            version);
    send(connfd, copy, strlen(copy), 0);
    logging(404, request, body, host, version, 0);
    close(connfd);
  } else {
    r = stat(body, &fs);

    // no permission on file
    if (r == -1) {
      sprintf(copy, "%s 403 Forbidden\r\nContent-Length:10\r\n\r\nForbidden\n",
              version);
      send(connfd, copy, strlen(copy), 0);
      logging(403, request, body, host, version, 0);
      close(connfd);
    } else {
      long fsize = fs.st_size;
      read_buffer = (char *)malloc(sizeof(char) * fsize);

      read_len = read(fd, read_buffer, fsize);

      // read_buffer[read_len] = '\0';

      sprintf(copy, "%s 200 OK\r\nContent-Length: %ld\r\n\r\n", version,
              read_len);
      send(connfd, copy, strlen(copy), 0);
      send(connfd, read_buffer, read_len, 0);

      logging(200, request, body, host, version, read_len);
      // 0 size byte
      if (fsize == 0) {
        sprintf(copy,
                "%s 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n",
                version);
        send(connfd, copy, strlen(copy), 0);
        logging(500, request, body, host, version, 0);
      }
    }
  }
  close(fd);
}

// handles PUT requese and response
// builds a response header
void send_put(int connfd, char *body, char *version, char *content_num,
              char *request, char *host) {
  char *copy;
  char *read_buffer;
  char *memory_buffer;
  int fd = 0;
  long write_len = 0, valread = 0, r = 0;
  struct stat fs;
  long fsize = atoi(content_num);
  long total = 0;

  copy = (char *)malloc(sizeof(char) * buffer_size);
  // read_buffer = (char *)malloc(sizeof(char) * buffer_size);

  memmove(&body[0], &body[1], strlen(body));

  fd = access(body, F_OK);
  // if file doesn't exist, we create one
  if (fd < 0) {
    fd = open(body, O_CREAT | O_WRONLY | O_TRUNC | O_APPEND, S_IRUSR | S_IWUSR);
    fsize = atoi(content_num);
    if (fsize > 0) {
      sprintf(copy, "%s 201 Created\r\nContent-Length: %ld\r\n\r\n", version,
              fsize);
      send(connfd, copy, strlen(copy), 0);

      read_buffer = (char *)malloc(sizeof(char) * fsize);
      memory_buffer = (char *)malloc(sizeof(char) * fsize);
      while (total < fsize) {
        valread = recv(connfd, read_buffer, fsize, 0);
        total += valread;
        write_len += write(fd, read_buffer, valread);
        memcpy(memory_buffer + total, read_buffer, valread);
        // send(connfd,read_buffer,write_len,0);
      }
      send(connfd, memory_buffer, write_len, 0);
      logging(200, request, body, host, version, total);
      // if able to write to a created file

    } else {
      sprintf(copy,
              "%s 500 Internal Server Error\r\nContent-Length: "
              "22\r\n\r\nInternal Server Error\n",
              version);
      send(connfd, copy, strlen(copy), 0);
      logging(500, request, body, host, version, 0);
    }
  } else {
    // r = access(body, R_OK | W_OK);
    r = stat(body, &fs);
    // if no permission on file
    if (r == -1) {
      sprintf(copy, "%s 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n",
              version);
      send(connfd, copy, strlen(copy), 0);
      logging(40, request, body, host, version, 0);
      close(connfd);
    } else {
      fd = open(body, O_CREAT | O_WRONLY | O_TRUNC | O_APPEND,
                S_IRUSR | S_IWUSR);

      fsize = atoi(content_num);
      if (fsize > 0) {
        sprintf(copy, "%s 200 OK\r\nContent-Length: %ld\r\n\r\n", version,
                fsize);
        send(connfd, copy, strlen(copy), 0);

        read_buffer = (char *)malloc(sizeof(char) * fsize);
        memory_buffer = (char *)malloc(sizeof(char) * fsize);
        while (total < fsize) {
          valread = recv(connfd, read_buffer, fsize, 0);
          total += valread;
          write_len += write(fd, read_buffer, valread);
          memcpy(memory_buffer + total, read_buffer, valread);
          // send(connfd,read_buffer,write_len,0);
        }
        send(connfd, memory_buffer, write_len, 0);
        logging(200, request, body, host, version, total);
      } else {

        sprintf(copy,
                "%s 500 Internal Server Error\r\nContent-Length: "
                "22\r\n\r\nInternal Server Error\n",
                version);
        send(connfd, copy, strlen(copy), 0);
        logging(500, request, body, host, version, 0);
      }
    }
  }
  close(fd);
}

// responsible for handling HEAD request and response
void send_head(int connfd, char *body, char *version, char *request,
               char *host) {
  char *read_buffer;
  char *copy;
  copy = (char *)malloc(sizeof(char) * buffer_size);

  long valread = 0, r = 0, infile = 0;
  struct stat fs;

  memmove(&body[0], &body[1], strlen(body));
  infile = open(body, O_RDONLY, S_IRUSR | S_IWUSR);
  if (infile < 0) {
    r = stat(body, &fs);
    if (r == -1) {
      sprintf(copy, "%s 403 Forbidden\r\nContent-Length: 10\r\n\r\n", version);
      send(connfd, copy, strlen(copy), 0);
      logging(403, request, body, host, version, 0);
    } else {
      sprintf(copy, "%s 404 Not Found\r\nContent-Length: 10\r\n\r\n", version);
      send(connfd, copy, strlen(copy), 0);
      logging(404, request, body, host, version, 0);
    }
  } else if (infile > 0) {
    r = stat(body, &fs);
    long fsize = fs.st_size;

    read_buffer = (char *)malloc(sizeof(char) * fsize);
    valread = read(infile, read_buffer, fsize);
    sprintf(copy, "%s 200 OK\r\nContent-Length: %ld\r\n\r\n", version, valread);
    send(connfd, copy, strlen(copy), 0);
    logging(200, request, body, host, version, valread);
  } else {
    sprintf(copy,
            "%s 500 Internal Server Error\r\nContent-Length: "
            "22\r\n\r\n",
            version);
    send(connfd, copy, strlen(copy), 0);
    logging(500, request, body, host, version, 0);
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
  char host_name[buffer_size];
  char host[buffer_size];
  char *p;

  while ((valread = recv(connfd, buffer, buffer_size, 0)) > 0) {

    printf("Job has started with connection:%d\n\n", connfd);

    write(STDOUT_FILENO, buffer, valread);
    // write(STDOUT_FILENO,"Hekki\n",6);

    sscanf(buffer, "%s %s %s %s %s", request, body, version, host_name, host);

    // checks to see if request is good
    if (strstr(version, "HTTP/1.1") == NULL || strlen(body) != 16 ||
        strstr(host_name, "Host") == NULL) {
      sprintf(copy,
              "%s 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n",
              version);
      send(connfd, copy, strlen(copy), 0);
      logging(400, request, body, host, version, 0);
      break;
    }
    // checks if file is ascii
    if (checker(body) == 0) {
      sprintf(copy,
              "%s 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n",
              version);
      send(connfd, copy, strlen(copy), 0);
      logging(400, request, body, host, version, 0);
      break;
    }
    // checks to see if connection is valid

    if (strcmp(request, "GET") == 0) {
      send_get(connfd, body, version, request, host);
    } else if (strcmp(request, "PUT") == 0) {
      p = strstr(buffer, "Content");

      if (p == NULL) {
        sprintf(copy,
                "%s 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n",
                version);
        send(connfd, copy, strlen(copy), 0);
        logging(400, request, body, host, version, 0);
      }
      sscanf(p, "%s %s", content, content_num);

      send_put(connfd, body, version, content_num, request, host);

    } else if (strcmp(request, "HEAD") == 0) {
      send_head(connfd, body, version, request, host);
    } else {
      sprintf(copy,
              "%s 501 Not Implemented\r\nContent-Length:16\r\n\r\nNot "
              "Implemented\n",
              version);
      send(connfd, copy, strlen(copy), 0);
      logging(501, request, body, host, version, 0);
    }
    printf("Job has finished with connection: %d \n\n", connfd);
    // pthread_mutex_unlock(&mutex);
    // sleep(1);
  }

  // when done, close socket
  close(connfd);
  return NULL;
}

// use shared buffer data structure
void *handle_thread(void *arg) {
  int thread = *((int *)arg);
  printf("Creating Thread: %d\n", thread);
  while (1) {
    int connfd = 0;
    pthread_mutex_lock(&lock);
    // dequeue

    if (s->size == 0) {
      pthread_cond_wait(&cond1, &lock);
    }
    if (s->size != 0) {
      s->count++;
      printf("Thread: %d has started working\n", thread);
      connfd = s->shared_buffer[s->front];
      s->size--;
      s->front++;
      if (s->front == s->capacity) {
        s->front = 0;
      }

      pthread_mutex_unlock(&lock);

      handle_connection(&connfd);
      printf("Thread: %d has finished. Waiting for new connection\n", thread);
      s->count--;
    } else {
      pthread_mutex_unlock(&lock);
    }
  }

  return NULL;
}

// add thread to stack buffer
void *thread_add(int connfd) {

  pthread_mutex_lock(&lock);
  // push to stack
  if (s->size != s->capacity) {

    s->size++;
    if (s->back == s->capacity) {
      s->back = 0;
    }
    s->shared_buffer[s->back] = connfd;
    s->back++;
  }

  pthread_mutex_unlock(&lock);
  pthread_cond_signal(&cond1);

  return NULL;
}

int main(int argc, char *argv[]) {
  int listenfd;
  uint16_t port;
  int opt;

  if (argc < 2) {
    errx(EXIT_FAILURE, "wrong arguments: %s port_num", argv[0]);
  }

  //  port = strtouint16(argv[1]);
  // if (port == 0) {
  // errx(EXIT_FAILURE, "invalid port number: %s", argv[1]);
  // }
  // listenfd = create_listen_socket(port);
  // get opt here

  int n = 4, i = 0;

  while (optind < argc) {
    i++;
    if ((opt = getopt(argc, argv, "N:l:")) != -1) {
      switch (opt) {
      case 'N':
        n = atoi(optarg);
        if (n == 0) {
          errx(EXIT_FAILURE, "thread size must be grater than 0");
        }
        break;
      case 'l':
        log_file =
            open(optarg, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
        if (log_file < 0) {
          errx(EXIT_FAILURE, "can't open log_file");
        }
        log_check = 1;
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
      if (port == 0) {
        errx(EXIT_FAILURE, "invalid port number: %s", argv[i]);
      }
    }
  }
  listenfd = create_listen_socket(port);

  pthread_t dispatcher[n];
  int worker[n];
  // pthread_mutex_init(&lock,NULL);
  // pthread_cond_init(&cond1,NULL);

  s = (stack *)malloc(sizeof(stack));
  s->size = 0;
  s->front = 0;
  s->back = 0;
  s->count = 0;
  s->capacity = 1024;
  s->shared_buffer = (int *)malloc(sizeof(int *) * 1024);

  for (int j = 0; j < n; j++) {
    worker[j] = j;
    pthread_create(&(dispatcher[j]), NULL, handle_thread, (void *)&worker[j]);
  }
  printf("Waiting for New Connection...\n");
  while (1) {
    // printf("Waiting for connection...\n");
    //  if (s->count != n) {
    int connfd = accept(listenfd, NULL, NULL);
    if (connfd < 0) {
      warn("accept error");
      continue;
    }

    thread_add(connfd);
    // }
  }
  return EXIT_SUCCESS;
}
