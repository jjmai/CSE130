# --------------------
#  Jordan Mai, jmai12
#  CSE130 Spring 2021, asgn1
#
#  Makefile for httpserver program
#
#  make
#  ---

CFLAGS = -Wall -Wextra -Wpedantic -Wshadow -std=c11
CC = clang $(CFLAGS)
MEMCHECK = valgrind --leak-check=full

all		:	httpserver

httpserver	:	httpserver.o
		$(CC) -o httpserver httpserver.o -g -lpthread

httpserver.o	:	httpserver.c
		$(CC) -c httpserver.c

clean		:
		rm -rf *.o httpserver
format		:
		clang-format -i -style=file *.c

valgrind	:
		MEMCHECK 
