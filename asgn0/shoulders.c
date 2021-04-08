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

  // error message for missing arguments
  if (argc < 2) {
    fprintf(stderr, "Error, no arugment for number of bytes detected\n");
    return -1;
  }
  int length = atoi(argv[1]);
  char buffer[length];

  // Base Case for no arguments input
  if (argc == 2) {
    n = read(infile, buffer, length);
    buffer[n] = '\0';
    write(outfile, buffer, n);
  } else {

    for (int i = 2; i < argc; i++) {
      if (strcmp(argv[i], "-") == 0) {
        n = read(STDIN_FILENO, buffer, length);
        buffer[n] = '\0';
      } else {

        infile = open(argv[i], O_RDWR | O_CREAT);
        // corrupted file
        if (infile < 0) {
          err(EXIT_FAILURE, "Error on opening file%s", argv[i]);
          return -1;
        }
        n = read(infile, buffer, length);
        buffer[n] = '\0';
      }
      write(outfile, buffer, n);
      close(infile);
    }
  }

  return 0;
}
