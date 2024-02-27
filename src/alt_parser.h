#ifndef MINMATH_ALT_PARSER_H__
#define MINMATH_ALT_PARSER_H__
#pragma once

#include "tokenizer.h"
#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AstNode *alt_parse_expression_from_string(const char *input);
struct AstNode *alt_parse_expression(struct Tokenizer *tokenizer, int min_precedence);
struct AstNode *alt_parse_increasing_precedence(struct Tokenizer *tokenizer, struct AstNode *left, int min_precedence);
struct AstNode *alt_parse_leaf(struct Tokenizer *tokenizer);

#ifdef __cplusplus
}
#endif

#endif
