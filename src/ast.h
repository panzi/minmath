#ifndef MINMATH_AST_H__
#define MINMATH_AST_H__
#pragma once

#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum NodeType {
    NODE_ADD,
    NODE_SUB,
    NODE_MUL,
    NODE_MOD,
    NODE_DIV,
    NODE_NEG,
    NODE_VAR,
    NODE_INT,
    NODE_IF,
    NODE_OR,
    NODE_AND,
    NODE_NOT,
    NODE_LT,
    NODE_GT,
    NODE_LE,
    NODE_GE,
    NODE_EQ,
    NODE_NE,
    NODE_BIT_OR,
    NODE_BIT_XOR,
    NODE_BIT_AND,
    NODE_BIT_NEG,
    NODE_LSHIFT,
    NODE_RSHIFT,
};

struct AstNode {
    enum NodeType type;
    union {
        int value;
        char *ident;
        struct AstNode *child;
        struct {
            struct AstNode *lhs;
            struct AstNode *rhs;
        } binary;
        struct {
            struct AstNode *cond;
            struct AstNode *then_expr;
            struct AstNode *else_expr;
        } terneary;
    } data;
};

struct Param {
    const char *name;
    int value;
};

struct AstNode *ast_create_terneary(struct AstNode *cond, struct AstNode *then_expr, struct AstNode *else_expr);
struct AstNode *ast_create_binary(enum NodeType type, struct AstNode *lhs, struct AstNode *rhs);
struct AstNode *ast_create_unary(enum NodeType type, struct AstNode *child);
struct AstNode *ast_create_int(int value);
struct AstNode *ast_create_var(char *name);
bool ast_is_binary(const struct AstNode *expr);
bool ast_is_unary(const struct AstNode *expr);
void ast_print(FILE *stream, const struct AstNode *expr);
void ast_free(struct AstNode *node);
int ast_execute_with_environ(struct AstNode *expr);

/// params need to be sorted
int ast_execute_with_params(struct AstNode *expr, const struct Param params[], size_t param_count);

void params_sort(struct Param params[], size_t param_count);
int params_get(const struct Param params[], size_t param_count, const char *name);

#ifdef __cplusplus
}
#endif

#endif
