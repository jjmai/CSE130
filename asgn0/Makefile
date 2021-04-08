# ----------------------------------------------
# Jordan Mai, jmai12
# CSE130 Spring 2021, asgn0
#
# Makefile for shoulders program
#
# make		compiles shoulders
# make clean	removes all binaries
# make format	auto fomrats all *.c *.h
# make infer	checks for errors
# make valgrind checks for memory leaks
# --------------------------------------------

CFLAGS = -Wall -std=c11
CC = clang $(CFLAGS)
MEMCHECK = valgrind --leak-check=full

all		:	shoulders

shoulders	:	shoulders.o
		$(CC) -o shoulders shoulders.o -g

shoulders.o	:	shoulders.c
		$(CC) -c shoulders.c

clean		:
		rm -rf *.o shoulders infer-out

format		:
		clang-format -i -style=file *.c 

infer		:
		make clean; infer-capture -- make; infer-analyze -- make

valgrind	:
		valgrind --leak-check=full shoulders

