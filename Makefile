CC = gcc
OBJ = minmath.o
BIN = minmath
CFLAGS = -Wall -Werror -std=c11 -g

.PHONY: all clean

all: minmath

%.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@

minmath: minmath.o
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rv $(OBJ) $(BIN)
