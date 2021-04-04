#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char *argv[]) {
  int infile = STDIN_FILENO;
  int outfile = STDOUT_FILENO;
  int length = atoi(argv[1]);
  char buffer[length];
  int n = 0;

  for (int i = 2; i < argc; i++) {
    if (strcmp(argv[i], "-") == 0) {
      n = read(STDIN_FILENO, buffer, length);
    } else {

      infile = open(argv[i], O_RDONLY | O_CREAT);
      n = read(infile, buffer, length);
      buffer[n] = '\0';
    }
    write(outfile, buffer, n);
  }
  printf("\n");
  close(infile);
}
