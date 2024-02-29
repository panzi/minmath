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
ALL_OBJ = $(TEST_OBJ) \
          build/main.o
BIN = build/minmath
TEST_BIN = build/minmath_test
CFLAGS = -Wall -Werror -std=gnu11
RELEASE = 0

ifeq ($(RELEASE),1)
      CFLAGS += -O3 -DNDEBUG
else
      CFLAGS += -g
endif

TESTDATA_CFLAGS = $(CFLAGS) -Wno-overflow -Wno-parentheses -Wno-logical-not-parentheses -Wno-bool-operation -Wno-div-by-zero

.PHONY: all clean test

all: $(BIN)

test: $(TEST_BIN)
	$(TEST_BIN)

build/testdata.o: src/testdata.c
	$(CC) $(TESTDATA_CFLAGS) $< -c -o $@

build/%.o: src/%.c
	$(CC) $(CFLAGS) $< -c -o $@

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

$(TEST_BIN): $(TEST_OBJ)
	$(CC) $(CFLAGS) $(TEST_OBJ) -o $@

clean:
	rm -rv $(ALL_OBJ) $(BIN) $(TEST_BIN) perf.data perf.data.old
