#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

int main(int argc, char *argv[]) {
  int infile = STDIN_FILENO;
  int outfile = STDOUT_FILENO;
  int buffer[100];
  int length = atoi(argv[1]);
  printf("%d" , length);
  int opt;

  while((opt = getopt(argc, argv,"")) !=EOF) {
    
  int n = read(infile, buffer,100);
}

  
  printf("Hello!\n");
}
