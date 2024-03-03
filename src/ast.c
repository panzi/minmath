#include "ast.h"

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#define AST_MALLOC() malloc(sizeof(struct AstNode))

bool ast_is_binary(const struct AstNode *expr) {
    switch (expr->type) {
        case NODE_ADD:
        case NODE_SUB:
        case NODE_MUL:
        case NODE_DIV:
        case NODE_MOD:
        case NODE_AND:
        case NODE_OR:
        case NODE_LT:
        case NODE_GT:
        case NODE_LE:
        case NODE_GE:
        case NODE_EQ:
        case NODE_NE:
        case NODE_BIT_AND:
        case NODE_BIT_OR:
        case NODE_BIT_XOR:
        case NODE_LSHIFT:
        case NODE_RSHIFT:
            return true;

        default:
            return false;
    }
}

bool ast_is_unary(const struct AstNode *expr) {
    switch (expr->type) {
        case NODE_NEG:
        case NODE_BIT_NEG:
        case NODE_NOT:
            return true;

        default:
            return false;
    }
}

struct AstNode *ast_create_terneary(struct AstNode *cond, struct AstNode *then_expr, struct AstNode *else_expr) {
    struct AstNode *node = AST_MALLOC();
    if (node == NULL) {
        return NULL;
    }

    *node = (struct AstNode){
        .type = NODE_IF,
        .data = {
            .terneary = {
                .cond      = cond,
                .then_expr = then_expr,
                .else_expr = else_expr,
            }
        }
    };

    return node;
}

struct AstNode *ast_create_binary(enum NodeType type, struct AstNode *lhs, struct AstNode *rhs) {
    assert(
        type == NODE_ADD ||
        type == NODE_SUB ||
        type == NODE_MUL ||
        type == NODE_DIV ||
        type == NODE_MOD ||
        type == NODE_OR ||
        type == NODE_AND ||
        type == NODE_LT ||
        type == NODE_GT ||
        type == NODE_LE ||
        type == NODE_GE ||
        type == NODE_EQ ||
        type == NODE_NE ||
        type == NODE_BIT_OR ||
        type == NODE_BIT_XOR ||
        type == NODE_BIT_AND ||
        type == NODE_LSHIFT ||
        type == NODE_RSHIFT
    );

    struct AstNode *node = AST_MALLOC();
    if (node == NULL) {
        return NULL;
    }

    *node = (struct AstNode){
        .type = type,
        .data = {
            .binary = {
                .lhs = lhs,
                .rhs = rhs,
            }
        }
    };

    return node;
}

struct AstNode *ast_create_unary(enum NodeType type, struct AstNode *child) {
    assert(type == NODE_NEG || type == NODE_BIT_NEG || type == NODE_NOT);

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
    if (node != NULL) {
        switch (node->type) {
            case NODE_ADD:
            case NODE_SUB:
            case NODE_MUL:
            case NODE_DIV:
            case NODE_MOD:
            case NODE_AND:
            case NODE_OR:
            case NODE_LT:
            case NODE_GT:
            case NODE_LE:
            case NODE_GE:
            case NODE_EQ:
            case NODE_NE:
            case NODE_BIT_AND:
            case NODE_BIT_OR:
            case NODE_BIT_XOR:
            case NODE_LSHIFT:
            case NODE_RSHIFT:
                ast_free(node->data.binary.lhs);
                ast_free(node->data.binary.rhs);
                break;

            case NODE_IF:
                ast_free(node->data.terneary.cond);
                ast_free(node->data.terneary.then_expr);
                ast_free(node->data.terneary.else_expr);
                break;

            case NODE_NEG:
            case NODE_BIT_NEG:
            case NODE_NOT:
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
}

void ast_print(FILE *stream, const struct AstNode *expr) {
    if (ast_is_binary(expr)) {
        fputc('(', stream);
        ast_print(stream, expr->data.binary.lhs);
        switch (expr->type) {
            case NODE_ADD:
                fprintf(stream, " + ");
                break;

            case NODE_SUB:
                fprintf(stream, " - ");
                break;

            case NODE_MUL:
                fprintf(stream, " * ");
                break;

            case NODE_DIV:
                fprintf(stream, " / ");
                break;

            case NODE_MOD:
                fprintf(stream, " %% ");
                break;

            case NODE_AND:
                fprintf(stream, " && ");
                break;

            case NODE_OR:
                fprintf(stream, " || ");
                break;

            case NODE_LT:
                fprintf(stream, " < ");
                break;

            case NODE_GT:
                fprintf(stream, " > ");
                break;

            case NODE_LE:
                fprintf(stream, " <= ");
                break;

            case NODE_GE:
                fprintf(stream, " >= ");
                break;

            case NODE_EQ:
                fprintf(stream, " == ");
                break;

            case NODE_NE:
                fprintf(stream, " != ");
                break;

            case NODE_BIT_AND:
                fprintf(stream, " & ");
                break;

            case NODE_BIT_OR:
                fprintf(stream, " | ");
                break;

            case NODE_BIT_XOR:
                fprintf(stream, " ^ ");
                break;

            case NODE_LSHIFT:
                fprintf(stream, " << ");
                break;

            case NODE_RSHIFT:
                fprintf(stream, " >> ");
                break;

            default:
                assert(false);
                fprintf(stream, " <?> ");
                break;
        }

        ast_print(stream, expr->data.binary.rhs);
        fputc(')', stream);
    } else if (expr->type == NODE_IF) {
        fputc('(', stream);
        ast_print(stream, expr->data.terneary.cond);
        fprintf(stream, " ? ");
        ast_print(stream, expr->data.terneary.then_expr);
        fprintf(stream, " : ");
        ast_print(stream, expr->data.terneary.else_expr);
        fputc(')', stream);
    } else if (ast_is_unary(expr)) {
        switch (expr->type) {
            case NODE_NEG:
                fprintf(stream, "- ");
                break;

            case NODE_BIT_NEG:
                fputc('~', stream);
                break;

            case NODE_NOT:
                fputc('!', stream);
                break;

            default:
                assert(false);
                fprintf(stream, "<?> ");
                break;
        }
        ast_print(stream, expr->data.child);
    } else if (expr->type == NODE_INT) {
        fprintf(stream, "%d", expr->data.value);
    } else if (expr->type == NODE_VAR) {
        fprintf(stream, "%s", expr->data.ident);
    } else {
        assert(false);
    }
}

// ========================================================================== //
//                                                                            //
//                             Tree Interpreter                               //
//                                                                            //
// ========================================================================== //

int ast_execute(struct AstNode *expr) {
    assert(expr != NULL);

    switch (expr->type) {
        case NODE_ADD:
            return (
                ast_execute(expr->data.binary.lhs) +
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_SUB:
            return (
                ast_execute(expr->data.binary.lhs) -
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_MUL:
            return (
                ast_execute(expr->data.binary.lhs) *
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_DIV:
            return (
                ast_execute(expr->data.binary.lhs) /
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_MOD:
            return (
                ast_execute(expr->data.binary.lhs) %
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_AND:
            return (
                ast_execute(expr->data.binary.lhs) &&
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_OR:
            return (
                ast_execute(expr->data.binary.lhs) ||
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_LT:
            return (
                ast_execute(expr->data.binary.lhs) <
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_GT:
            return (
                ast_execute(expr->data.binary.lhs) >
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_LE:
            return (
                ast_execute(expr->data.binary.lhs) <=
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_GE:
            return (
                ast_execute(expr->data.binary.lhs) >=
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_EQ:
            return (
                ast_execute(expr->data.binary.lhs) ==
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_NE:
            return (
                ast_execute(expr->data.binary.lhs) !=
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_BIT_AND:
            return (
                ast_execute(expr->data.binary.lhs) &
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_BIT_OR:
            return (
                ast_execute(expr->data.binary.lhs) |
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_BIT_XOR:
            return (
                ast_execute(expr->data.binary.lhs) ^
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_LSHIFT:
            return (
                ast_execute(expr->data.binary.lhs) <<
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_RSHIFT:
            return (
                ast_execute(expr->data.binary.lhs) >>
                ast_execute(expr->data.binary.rhs)
            );

        case NODE_NEG:
            return -ast_execute(expr->data.child);

        case NODE_BIT_NEG:
            return ~ast_execute(expr->data.child);

        case NODE_NOT:
            return !ast_execute(expr->data.child);

        case NODE_IF:
            return (
                ast_execute(expr->data.terneary.cond) ?
                ast_execute(expr->data.terneary.then_expr) :
                ast_execute(expr->data.terneary.else_expr)
            );

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
