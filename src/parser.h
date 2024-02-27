#ifndef MINMATH_PARSER_H__
#define MINMATH_PARSER_H__
#pragma once

#include "tokenizer.h"
#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

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
