#ifndef MINMATH_TOKENIZER_H__
#define MINMATH_TOKENIZER_H__
#pragma once

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum TokenType {
    TOK_START = 0,
    TOK_EOF   = -1,
    TOK_ERROR_TOKEN  = -2,
    TOK_ERROR_MEMORY = -3,
    TOK_PLUS  = '+',
    TOK_MINUS = '-',
    TOK_MUL   = '*',
    TOK_DIV   = '/',
    TOK_INT   = '0',
    TOK_IDENT = 'a',
    TOK_OPEN_PAREN  = '(',
    TOK_CLOSE_PAREN = ')',
};

struct Tokenizer {
    const char *input;
    size_t input_pos;
    size_t token_pos;

    enum TokenType token;
    bool peeked;
    int value;
    char *ident;
};

#define TOKENIZER_INIT(INPUT) {  \
    .input  = (INPUT),           \
    .input_pos = 0,              \
    .token_pos = 0,              \
    .token  = TOK_START,         \
    .peeked = false,             \
    .value  = -1,                \
    .ident  = NULL,              \
}

enum TokenType peek_token(struct Tokenizer *tokenizer);
enum TokenType next_token(struct Tokenizer *tokenizer);
bool token_is_error(enum TokenType token);
void tokenizer_free(struct Tokenizer *tokenizer);

#ifdef __cplusplus
}
#endif

#endif
