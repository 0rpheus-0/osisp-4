CC = gcc
CFLAGS = -W -Wall -Wno-unused-parameter -Wno-unused-variable -std=c11 -pedantic
.PHONY:clean

all: main 

run: main
	./main
	
main: main.c Makefile
	$(CC) $(CFLAGS) main.c -o main

clean:
	rm -f main