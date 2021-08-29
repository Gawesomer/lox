CC	 = gcc
CFLAGS	 = -g -ggdb3
CPPFLAGS = -Wall -pedantic

ARFLAGS  = rvu

.PHONEY: all clean

all: main

main: main.o chunk.o debug.o memory.o

clean: 
	rm -f *.o *.a main
