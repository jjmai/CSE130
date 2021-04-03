CFLAGS = -Wall -std=c11
CC = clang $(CFLAGS)

all		:	shoulders

shoulders	:	shoulders.o
		$(CC) -o shoulders shoulders.o -g

shoulders.o	:	shoulders.c
		$(CC) -c shoulders.c

clean		:
		rm -rf *.o shoulders

format		:
		clang-format -i -style=file *.c
