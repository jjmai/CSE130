# --------------------
#  Jordan Mai, jmai12
#  CSE130 Spring 2021, asgn1
#
#  Makefile for httpserver program
#
#  make
#  -----------------------------

CFLAGS = -Wall -Wextra -Wpedantic -Wshadow -std=c11
CC = clang $(CFLAGS)
MEMCHECK = valgrind --leak-check=full

all		:	httpproxy

httpproxy	:	httpproxy.o
		$(CC) -o httpproxy httpproxy.o -g 

httpproxy.o	:	httpproxy.c
		$(CC) -c httpproxy.c -g

clean		:
		rm -rf *.o httpproxy
format		:
		clang-format -i -style=file *.c


