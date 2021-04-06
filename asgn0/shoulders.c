#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char *argv[]) {
  int infile = STDIN_FILENO;
  int outfile = STDOUT_FILENO;
  int n = 0;
  if (argc < 2) {
    fprintf(stderr, "Error, no arugments detected\n");
    // err(EXIT_FAILURE, "Error, no arguments");
    return -1;
  }
  int length = atoi(argv[1]);
  char buffer[length];
  // char buffer_write[length * argc];
  // int count = 0;

  // Base Case for no arguments input
  if (argc == 2) {
    n = read(infile, buffer, length);
    buffer[n] = '\0';
    write(outfile, buffer, n);
  } else {

    for (int i = 2; i < argc; i++) {
      if (strcmp(argv[i], "-") == 0) {
        n = read(STDIN_FILENO, buffer, length);
        buffer[n]='\0';
      } else {

        infile = open(argv[i], O_RDONLY | O_CREAT);
        if (infile == -1) {
          err(EXIT_FAILURE, "Error on opening file%s", argv[i]);
          return -1;
        }
        n = read(infile, buffer, length);
        //  count += n;
        buffer[n] = '\0';
      }
      write(outfile, buffer, n);
      close(infile);
    }
  }
  // write(outfile, buffer, count);
  return 0;
}
