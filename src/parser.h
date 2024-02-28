#ifndef MINMATH_PARSER_H__
#define MINMATH_PARSER_H__
#pragma once

#include "tokenizer.h"
#include "parser_error.h"
#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Parser {
    struct Tokenizer tokenizer;
    struct ErrorInfo error;
};

#define PARSER_INIT(INPUT) {             \
    .tokenizer = TOKENIZER_INIT(INPUT),  \
    .error = {                           \
        .error  = PARSER_ERROR_OK,       \
        .offset = 0,                     \
        .context_offset = 0,             \
    },                                   \
}

struct AstNode *parse_expression_from_string(const char *input, struct ErrorInfo *error);
struct AstNode *parse_expression(struct Parser *parser);
struct AstNode *parse_sum(struct Parser *parser);
struct AstNode *parse_product(struct Parser *parser);
struct AstNode *parse_unary(struct Parser *parser);
struct AstNode *parse_atom(struct Parser *parser);
void parser_free(struct Parser *parser);

#ifdef __cplusplus
}
#endif

#endif
