#include <stddef.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "optimizer.h"

static inline bool ast_is_boolean(const struct AstNode *expr) {
    switch (expr->type) {
        case NODE_NOT:
        case NODE_EQ:
        case NODE_NE:
        case NODE_LT:
        case NODE_GT:
        case NODE_LE:
        case NODE_GE:
            return true;

        default:
            return false;
    }
}

static inline struct AstNode *ast_create_bool(struct AstNode *child) {
    if (ast_is_boolean(child)) {
        return child;
    }

    struct AstNode *bool1 = ast_create_unary(NODE_NOT, child);
    if (bool1 == NULL) {
        ast_free(child);
        return NULL;
    }

    struct AstNode *bool2 = ast_create_unary(NODE_NOT, bool1);
    if (bool2 == NULL) {
        ast_free(bool1);
        return NULL;
    }

    return bool2;
}

// This optimizer just does simple constant folding.
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
            if (expr->type == NODE_AND || expr->type == NODE_OR) {
                struct AstNode *tmp;
                if (lhs->type == NODE_NOT && lhs->data.child->type == NODE_NOT) {
                    tmp = lhs->data.child;
                    lhs->data.child = NULL;
                    ast_free(lhs);
                    lhs = tmp;
                }

                if (rhs->type == NODE_NOT && rhs->data.child->type == NODE_NOT) {
                    tmp = rhs->data.child;
                    rhs->data.child = NULL;
                    ast_free(rhs);
                    rhs = tmp;
                }
            }

            if (
                (expr->type == NODE_ADD || expr->type == NODE_SUB || expr->type == NODE_BIT_OR) && rhs->type == NODE_INT && rhs->data.value == 0
            ) {
                ast_free(rhs);
                return lhs;
            } else if (
                (expr->type == NODE_ADD || expr->type == NODE_BIT_OR) && lhs->type == NODE_INT && lhs->data.value == 0
            ) {
                ast_free(lhs);
                return rhs;
            } else if (
                expr->type == NODE_OR && rhs->type == NODE_INT && rhs->data.value == 0
            ) {
                ast_free(rhs);
                return ast_create_bool(lhs);
            } else if (
                expr->type == NODE_OR && lhs->type == NODE_INT && lhs->data.value == 0
            ) {
                ast_free(lhs);
                return ast_create_bool(rhs);
            } else if (
                expr->type == NODE_OR && (
                    (rhs->type == NODE_INT && rhs->data.value != 0) ||
                    (lhs->type == NODE_INT && lhs->data.value != 0)
                )
            ) {
                ast_free(rhs);
                ast_free(lhs);
                return ast_create_int(1);
            } else if (
                expr->type == NODE_AND && rhs->type == NODE_INT && rhs->data.value != 0
            ) {
                ast_free(rhs);
                return ast_create_bool(lhs);
            } else if (
                expr->type == NODE_AND && lhs->type == NODE_INT && lhs->data.value != 0
            ) {
                ast_free(lhs);
                return ast_create_bool(rhs);
            } else if (
                expr->type == NODE_SUB && lhs->type == NODE_INT && lhs->data.value == 0
            ) {
                ast_free(lhs);
                struct AstNode *opt_expr = ast_create_unary(NODE_NEG, rhs);
                if (opt_expr == NULL) {
                    ast_free(rhs);
                    return NULL;
                }
                return opt_expr;
            } else if (
                ((expr->type == NODE_MUL || expr->type == NODE_AND) && ((lhs->type == NODE_INT && lhs->data.value == 0) || (rhs->type == NODE_INT && rhs->data.value == 0))) ||
                ((expr->type == NODE_DIV || expr->type == NODE_MOD) && lhs->type == NODE_INT && lhs->data.value == 0)
            ) {
                ast_free(lhs);
                ast_free(rhs);
                return ast_create_int(0);
            } else if (
                expr->type == NODE_EQ && lhs->type == NODE_INT && lhs->data.value == 0
            ) {
                ast_free(lhs);
                struct AstNode *opt_expr = ast_create_unary(NODE_NOT, rhs);
                if (opt_expr == NULL) {
                    ast_free(rhs);
                    return NULL;
                }
                return opt_expr;
            } else if (
                expr->type == NODE_EQ && rhs->type == NODE_INT && rhs->data.value == 0
            ) {
                ast_free(rhs);
                struct AstNode *opt_expr = ast_create_unary(NODE_NOT, lhs);
                if (opt_expr == NULL) {
                    ast_free(lhs);
                    return NULL;
                }
                return opt_expr;
            } else {
                struct AstNode *opt_expr = ast_create_binary(expr->type, lhs, rhs);

                if (opt_expr == NULL) {
                    ast_free(lhs);
                    ast_free(rhs);
                    return NULL;
                }

                return opt_expr;
            }
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

        if (cond_expr->type == NODE_NE) {
            if (
                cond_expr->data.binary.lhs->type == NODE_INT && cond_expr->data.binary.lhs->data.value == 0
            ) {
                struct AstNode *rhs = cond_expr->data.binary.rhs;
                cond_expr->data.binary.rhs = NULL;
                ast_free(cond_expr);
                cond_expr = rhs;
            } else if (
                cond_expr->data.binary.rhs->type == NODE_INT && cond_expr->data.binary.rhs->data.value == 0
            ) {
                struct AstNode *lhs = cond_expr->data.binary.lhs;
                cond_expr->data.binary.lhs = NULL;
                ast_free(cond_expr);
                cond_expr = lhs;
            }
        } else if (cond_expr->type == NODE_NOT) {
            if (cond_expr->data.child->type == NODE_NOT) {
                struct AstNode *tmp = cond_expr->data.child->data.child;
                cond_expr->data.child->data.child = NULL;
                ast_free(cond_expr);
                cond_expr = tmp;
            } else {
                struct AstNode *tmp = cond_expr->data.child;
                cond_expr->data.child = NULL;
                ast_free(cond_expr);
                cond_expr = tmp;
                tmp = then_expr;
                then_expr = else_expr;
                else_expr = tmp;
            }
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
        } else if (
            (expr->type == NODE_BIT_NEG && child->type == NODE_BIT_NEG) ||
            (expr->type == NODE_NEG && child->type == NODE_NEG) ||
            (expr->type == NODE_NOT && child->type == NODE_NOT && ast_is_boolean(child->data.child))
        ) {
            struct AstNode *new_expr = child->data.child;
            child->data.child = NULL;
            ast_free(child);
            return new_expr;
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
