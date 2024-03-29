#ifndef MINMATH_FAST_PARSER_H__
#define MINMATH_FAST_PARSER_H__
#pragma once

#include "tokenizer.h"
#include "ast.h"
#include "parser_error.h"

#ifdef __cplusplus
extern "C" {
#endif

struct FastParser {
    struct Tokenizer tokenizer;
    struct ErrorInfo error;
};

#define FAST_PARSER_INIT(INPUT) {        \
    .tokenizer = TOKENIZER_INIT(INPUT),  \
    .error = {                           \
        .error  = PARSER_ERROR_OK,       \
        .offset = 0,                     \
        .context_offset = 0,             \
        .token  = TOK_EOF,               \
    },                                   \
}

struct AstNode *fast_parse(const char *input, struct ErrorInfo *error);
void fast_parser_free(struct FastParser *parser);

#ifdef __cplusplus
}
#endif

#endif
