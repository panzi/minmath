#include "ast.h"

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#define AST_MALLOC() malloc(sizeof(struct AstNode))

struct AstNode *ast_create_binary(enum NodeType type, struct AstNode *lhs, struct AstNode *rhs) {
    assert(type == NODE_ADD || type == NODE_SUB || type == NODE_MUL || type == NODE_DIV);

    struct AstNode *node = AST_MALLOC();
    if (node == NULL) {
        return NULL;
    }

    *node = (struct AstNode){
        .type = type,
        .data = {
            .children = {
                .lhs = lhs,
                .rhs = rhs,
            }
        }
    };

    return node;
}

struct AstNode *ast_create_unary(enum NodeType type, struct AstNode *child) {
    assert(type == NODE_NEG);

    struct AstNode *node = AST_MALLOC();
    if (node == NULL) {
        return NULL;
    }

    *node = (struct AstNode){
        .type = type,
        .data = {
            .child = child
        }
    };

    return node;
}

struct AstNode *ast_create_int(int value) {
    struct AstNode *node = AST_MALLOC();
    if (node == NULL) {
        return NULL;
    }

    *node = (struct AstNode){
        .type = NODE_INT,
        .data = {
            .value = value
        }
    };

    return node;
}

struct AstNode *ast_create_var(char *name) {
    struct AstNode *node = AST_MALLOC();
    if (node == NULL) {
        return NULL;
    }

    *node = (struct AstNode){
        .type = NODE_VAR,
        .data = {
            .ident = name
        }
    };

    return node;
}

void ast_free(struct AstNode *node) {
    switch (node->type) {
        case NODE_ADD:
        case NODE_SUB:
        case NODE_MUL:
        case NODE_DIV:
            ast_free(node->data.children.lhs);
            ast_free(node->data.children.rhs);
            break;

        case NODE_NEG:
            ast_free(node->data.child);
            break;

        case NODE_VAR:
            free(node->data.ident);
            break;

        case NODE_INT:
            break;
    }
    free(node);
}

// ========================================================================== //
//                                                                            //
//                             Tree Interpreter                               //
//                                                                            //
// ========================================================================== //

int ast_execute(struct AstNode *expr) {
    switch (expr->type) {
        case NODE_ADD:
            return (
                ast_execute(expr->data.children.lhs) +
                ast_execute(expr->data.children.rhs)
            );

        case NODE_SUB:
            return (
                ast_execute(expr->data.children.lhs) -
                ast_execute(expr->data.children.rhs)
            );

        case NODE_MUL:
            return (
                ast_execute(expr->data.children.lhs) *
                ast_execute(expr->data.children.rhs)
            );

        case NODE_DIV:
            return (
                ast_execute(expr->data.children.lhs) /
                ast_execute(expr->data.children.rhs)
            );

        case NODE_NEG:
            return -ast_execute(expr->data.child);

        case NODE_INT:
            return expr->data.value;

        case NODE_VAR:
        {
            const char *val = getenv(expr->data.ident);
            if (val == NULL) {
                return 0;
            }
            return atoi(val);
        }
        default:
            assert(false);
            return 0;
    }
}
