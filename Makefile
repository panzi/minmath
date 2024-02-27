CC = gcc
SHARED_OBJ = build/alt_parser.o \
             build/ast.o \
             build/parser.o \
             build/tokenizer.o \
             build/parser_error.o
OBJ = $(SHARED_OBJ) \
      build/main.o
TEST_OBJ = $(SHARED_OBJ) \
           build/testdata.o \
           build/test.o
BIN = build/minmath
TEST_BIN = build/minmath_test
CFLAGS = -Wall -Werror -std=gnu11 -O2 -Wno-overflow
RELEASE = 0

ifeq ($(RELEASE),1)
CFLAGS += -DNDEBUG
else
CFLAGS += -g
endif

.PHONY: all clean test

all: $(BIN)

test: $(TEST_BIN)
	$(TEST_BIN)

build/%.o: src/%.c
	$(CC) $(CFLAGS) $< -c -o $@

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

$(TEST_BIN): $(TEST_OBJ)
	$(CC) $(CFLAGS) $(TEST_OBJ) -o $@

clean:
	rm -rv $(OBJ) $(TEST_OBJ) $(BIN) $(TEST_BIN) perf.data perf.data.old
