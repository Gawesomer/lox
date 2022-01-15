CC	     = gcc
CFLAGS	 = -g -ggdb3 -O0 -pg
CPPFLAGS = -Wall -pedantic
LDFLAGS  = -pg # Profiling reference: http://web.archive.org/web/20141129061523/http://www.cs.utah.edu/dept/old/texinfo/as/gprof.html#SEC2
ARFLAGS  = rvu

.PHONEY: all clean

all: main

main: main.o chunk.o compiler.o debug.o line.o memory.o object.o scanner.o table.o value.o vm.o

clean:
	rm -f *.o *.a *.~ main
