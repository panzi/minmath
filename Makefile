CC = gcc

SHARED_CFLAGS = -Wall -Werror -std=gnu11
DEBUG_CFLAGS = $(SHARED_CFLAGS) -g
TEST_CFLAGS = $(SHARED_CFLAGS) -O3 -DNDEBUG -g
RELEASE_CFLAGS = $(SHARED_CFLAGS) -O3 -DNDEBUG
CFLAGS ?= $(DEBUG_CFLAGS)

BUILD_TYPE ?= debug

.PHONY: all clean test perf

ifeq ($(BUILD_TYPE),release)
      CFLAGS = $(RELEASE_CFLAGS)
else
ifeq ($(BUILD_TYPE),test)
      CFLAGS = $(TEST_CFLAGS)
else
      CFLAGS = $(DEBUG_CFLAGS)
endif
endif

SHARED_OBJ = build/$(BUILD_TYPE)/alt_parser.o \
             build/$(BUILD_TYPE)/ast.o \
             build/$(BUILD_TYPE)/parser.o \
             build/$(BUILD_TYPE)/tokenizer.o \
             build/$(BUILD_TYPE)/parser_error.o \
             build/$(BUILD_TYPE)/optimizer.o \
             build/$(BUILD_TYPE)/bytecode.o
OBJ = $(SHARED_OBJ) \
      build/$(BUILD_TYPE)/main.o
TEST_OBJ = $(SHARED_OBJ) \
           build/$(BUILD_TYPE)/testdata.o \
           build/$(BUILD_TYPE)/test.o
ALL_OBJ = $(TEST_OBJ) \
          build/$(BUILD_TYPE)/main.o
BIN = build/$(BUILD_TYPE)/minmath
TEST_BIN = build/$(BUILD_TYPE)/minmath_test

TESTDATA_CFLAGS = $(CFLAGS) -O1 -Wno-overflow -Wno-parentheses -Wno-logical-not-parentheses -Wno-bool-operation -Wno-div-by-zero -Wno-shift-count-overflow -Wno-shift-overflow -Wno-shift-count-negative

all: $(BIN)

test: $(TEST_BIN)
	$(TEST_BIN)

perf: $(TEST_BIN)
	perf record $(TEST_BIN)
	perf report

build/$(BUILD_TYPE)/testdata.o: src/testdata.c
	$(CC) $(TESTDATA_CFLAGS) $< -c -o $@

build/$(BUILD_TYPE)/%.o: src/%.c
	$(CC) $(CFLAGS) $< -c -o $@

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

$(TEST_BIN): $(TEST_OBJ)
	$(CC) $(CFLAGS) $(TEST_OBJ) -o $@

clean:
	rm -rv $(ALL_OBJ) $(BIN) $(TEST_BIN) perf.data perf.data.old
