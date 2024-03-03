#ifndef MINMATH_TOKENIZER_H__
#define MINMATH_TOKENIZER_H__
#pragma once

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum TokenType {
    TOK_START   = 0,
    TOK_EOF     = -1,
    TOK_ERROR_TOKEN  = -2,
    TOK_ERROR_MEMORY = -3,
    TOK_PLUS    = '+',
    TOK_MINUS   = '-',
    TOK_MUL     = '*',
    TOK_DIV     = '/',
    TOK_MOD     = '%',
    TOK_INT     = '0',
    TOK_IDENT   = 'a',
    TOK_LPAREN  = '(',
    TOK_RPAREN  = ')',
    TOK_QUEST   = '?',
    TOK_COLON   = ':',
    TOK_BIT_OR  = '|',
    TOK_BIT_XOR = '^',
    TOK_BIT_AND = '&',
    TOK_BIT_NEG = '~',
    TOK_NOT     = '!',
    TOK_AND     = ('&' << 8) | '&',
    TOK_OR      = ('|' << 8) | '|',
    TOK_LT      = '<',
    TOK_GT      = '>',
    TOK_LE      = ('<' << 8) | '=',
    TOK_GE      = ('>' << 8) | '=',
    TOK_EQ      = ('=' << 8) | '=',
    TOK_NE      = ('!' << 8) | '=',
    TOK_LSHIFT  = ('<' << 8) | '<',
    TOK_RSHIFT  = ('>' << 8) | '>',
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
const char *get_token_name(enum TokenType token);

#ifdef __cplusplus
}
#endif

#endif
