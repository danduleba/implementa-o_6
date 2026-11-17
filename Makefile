CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic

.PHONY: all clean

all: banker

banker: banker.c
	$(CC) $(CFLAGS) banker.c -o banker

clean:
	rm -f banker
