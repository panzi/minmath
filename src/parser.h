#ifndef MINMATH_PARSER_H__
#define MINMATH_PARSER_H__
#pragma once

#include "tokenizer.h"
#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

enum ParserError {
    PARSER_ERROR_OK = 0,
    PARSER_ERROR_MEMORY,
    PARSER_ERROR_ILLEGAL_TOKEN,
    PARSER_ERROR_UNEXPECTED_EOF,
};

struct ErrorInfo {
    enum ParserError error;
    size_t offset;
    size_t context_offset;
};

struct Parser {
    struct Tokenizer tokenizer;
    struct ErrorInfo error;
};

struct SourceLocation {
    size_t lineno;
    size_t column;
};

struct AstNode *parse_expression_from_string(const char *input);
struct AstNode *parse_expression(struct Tokenizer *tokenizer);
struct AstNode *parse_sum(struct Tokenizer *tokenizer);
struct AstNode *parse_product(struct Tokenizer *tokenizer);
struct AstNode *parse_unary(struct Tokenizer *tokenizer);
struct AstNode *parse_atom(struct Tokenizer *tokenizer);

#ifdef __cplusplus
}
#endif

#endif
