#ifndef MINMATH_ALT_PARSER_H__
#define MINMATH_ALT_PARSER_H__
#pragma once

#include "tokenizer.h"
#include "ast.h"
#include "parser_error.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AltParser {
    struct Tokenizer tokenizer;
    struct ErrorInfo error;
};

#define ALT_PARSER_INIT(INPUT) {         \
    .tokenizer = TOKENIZER_INIT(INPUT),  \
    .error = {                           \
        .error  = PARSER_ERROR_OK,       \
        .offset = 0,                     \
        .context_offset = 0,             \
        .token  = TOK_EOF,               \
    },                                   \
}

struct AstNode *alt_parse_expression_from_string(const char *input, struct ErrorInfo *error);
struct AstNode *alt_parse_expression(struct AltParser *parser, int min_precedence);
struct AstNode *alt_parse_increasing_precedence(struct AltParser *parser, struct AstNode *left, int min_precedence);
struct AstNode *alt_parse_leaf(struct AltParser *parser);
void alt_parser_free(struct AltParser *parser);

#ifdef __cplusplus
}
#endif

#endif
