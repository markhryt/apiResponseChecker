CC=gcc
CFLAGS=-g -Wall

timer: timer.o

timer.o: timer.c

.PHONY: clean

clean:
	rm -rf *.o timer
