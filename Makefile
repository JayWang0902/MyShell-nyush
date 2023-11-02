CC=gcc
CFLAGS=-g -pedantic -std=gnu17 -Wall -Wextra

.PHONY: all
all: nyush

nyush: nyush.o shell.o

nyush.o: nyush.c nyush.h

shell.o: shell.c nyush.h

clean:
	rm -f *.o nyush