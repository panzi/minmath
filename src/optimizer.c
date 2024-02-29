#include <stddef.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "optimizer.h"

struct AstNode *ast_optimize(const struct AstNode *expr) {
    assert(expr != NULL);

    if (ast_is_binary(expr)) {
        struct AstNode *lhs = ast_optimize(expr->data.binary.lhs);
        struct AstNode *rhs = ast_optimize(expr->data.binary.rhs);

        if (lhs == NULL || rhs == NULL) {
            ast_free(lhs);
            ast_free(rhs);
            return NULL;
        }

        if (lhs->type == NODE_INT && rhs->type == NODE_INT) {
            switch (expr->type) {
                case NODE_ADD:
                    lhs->data.value += rhs->data.value;
                    break;

                case NODE_SUB:
                    lhs->data.value -= rhs->data.value;
                    break;

                case NODE_MUL:
                    lhs->data.value *= rhs->data.value;
                    break;

                case NODE_DIV:
                    lhs->data.value /= rhs->data.value;
                    break;

                case NODE_MOD:
                    lhs->data.value %= rhs->data.value;
                    break;

                case NODE_AND:
                    lhs->data.value = lhs->data.value && rhs->data.value;
                    break;

                case NODE_OR:
                    lhs->data.value = lhs->data.value || rhs->data.value;
                    break;

                case NODE_LT:
                    lhs->data.value = lhs->data.value < rhs->data.value;
                    break;

                case NODE_GT:
                    lhs->data.value = lhs->data.value > rhs->data.value;
                    break;

                case NODE_LE:
                    lhs->data.value = lhs->data.value <= rhs->data.value;
                    break;

                case NODE_GE:
                    lhs->data.value = lhs->data.value >= rhs->data.value;
                    break;

                case NODE_EQ:
                    lhs->data.value = lhs->data.value == rhs->data.value;
                    break;

                case NODE_NE:
                    lhs->data.value = lhs->data.value != rhs->data.value;
                    break;

                case NODE_BIT_AND:
                    lhs->data.value &= rhs->data.value;
                    break;

                case NODE_BIT_OR:
                    lhs->data.value |= rhs->data.value;
                    break;

                case NODE_BIT_XOR:
                    lhs->data.value ^= rhs->data.value;
                    break;

                default:
                    assert(false);
                    ast_free(lhs);
                    ast_free(rhs);
                    errno = EINVAL;
                    return NULL;
            }

            ast_free(rhs);
            return lhs;
        } else {
            struct AstNode *opt_expr = ast_create_binary(expr->type, lhs, rhs);

            if (opt_expr == NULL) {
                ast_free(lhs);
                ast_free(rhs);
                return NULL;
            }

            return opt_expr;
        }
    } else if (expr->type == NODE_IF) {
        struct AstNode *cond_expr = ast_optimize(expr->data.terneary.cond);

        if (cond_expr == NULL) {
            return NULL;
        }

        if (cond_expr->type == NODE_INT) {
            int cond_value = cond_expr->data.value;
            ast_free(cond_expr);
            if (cond_value) {
                return ast_optimize(expr->data.terneary.then_expr);
            } else {
                return ast_optimize(expr->data.terneary.else_expr);
            }
        }

        struct AstNode *then_expr = ast_optimize(expr->data.terneary.then_expr);

        if (then_expr == NULL) {
            ast_free(cond_expr);
            return NULL;
        }

        struct AstNode *else_expr = ast_optimize(expr->data.terneary.else_expr);

        if (else_expr == NULL) {
            ast_free(cond_expr);
            ast_free(then_expr);
            return NULL;
        }

        struct AstNode *if_expr = ast_create_terneary(cond_expr, then_expr, else_expr);

        if (if_expr == NULL) {
            ast_free(cond_expr);
            ast_free(then_expr);
            ast_free(else_expr);
            return NULL;
        }

        return if_expr;
    } else if (ast_is_unary(expr)) {
        struct AstNode *child = ast_optimize(expr->data.child);

        if (child == NULL) {
            return NULL;
        }

        if (child->type == NODE_INT) {
            switch (expr->type) {
            case NODE_NEG:
                child->data.value = -child->data.value;
                break;

            case NODE_BIT_NEG:
                child->data.value = ~child->data.value;
                break;

            case NODE_NOT:
                child->data.value = !child->data.value;
                break;

            default:
                assert(false);
                ast_free(child);
                errno = EINVAL;
                return NULL;
            }
            return child;
        }

        struct AstNode *unary_expr = ast_create_unary(expr->type, child);

        if (unary_expr == NULL) {
            ast_free(child);
            return NULL;
        }

        return unary_expr;
    } else if (expr->type == NODE_INT) {
        return ast_create_int(expr->data.value);
    } else if (expr->type == NODE_VAR) {
        char *name = strdup(expr->data.ident);
        if (name == NULL) {
            return NULL;
        }
        return ast_create_var(name);
    } else {
        assert(false);
        errno = EINVAL;
        return NULL;
    }
}
