CC = gcc
OBJ = minmath.o
BIN = minmath
CFLAGS = -Wall -Werror -std=gnu11 -g -O2 -Wno-overflow

.PHONY: all clean test

all: minmath

test: minmath_test
	./minmath_test

%.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@

minmath: minmath.o
	$(CC) $(CFLAGS) $< -o $@

minmath_test: minmath.c
	$(CC) $(CFLAGS) -DTEST=1 $< -o $@

clean:
	rm -rv $(OBJ) $(BIN) minmath_test perf.data perf.data.old
