CC	 = gcc
CFLAGS	 = -g -ggdb3
CPPFLAGS = -Wall -pedantic

ARFLAGS  = rvu

.PHONEY: all clean

all: main

main: main.o chunk.o compiler.o debug.o line.o memory.o object.o scanner.o value.o vm.o

clean:
	rm -f *.o *.a *.~ main
