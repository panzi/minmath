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

static inline struct AstNode *ast_create_bool(struct AstBuffer *buffer, struct AstNode *child) {
    if (ast_is_boolean(child)) {
        return child;
    }

    struct AstNode *bool1 = ast_create_unary(buffer, NODE_NOT, child);
    if (bool1 == NULL) {
        return NULL;
    }

    struct AstNode *bool2 = ast_create_unary(buffer, NODE_NOT, bool1);
    if (bool2 == NULL) {
        return NULL;
    }

    return bool2;
}

static inline int factor_to_shift_count(int factor) {
    for (int bit_pos = 0; bit_pos < 32; ++ bit_pos) {
        if (factor & (1 << bit_pos)) {
            if (factor & ~(1 << bit_pos)) {
                return 0;
            } else {
                return bit_pos;
            }
        }
    }
    return 0;
}

// This optimizer just does simple constant folding.
struct AstNode *ast_optimize(struct AstBuffer *buffer, const struct AstNode *expr) {
    assert(expr != NULL);

    if (ast_is_binary(expr)) {
        struct AstNode *lhs = ast_optimize(buffer, expr->data.binary.lhs);
        struct AstNode *rhs = ast_optimize(buffer, expr->data.binary.rhs);

        if (lhs == NULL || rhs == NULL) {
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
                    if (rhs->data.value == 0) {
                        struct AstNode *opt_expr = ast_create_binary(buffer, expr->type, lhs, rhs);

                        if (opt_expr == NULL) {
                            return NULL;
                        }

                        return opt_expr;
                    }
                    lhs->data.value /= rhs->data.value;
                    break;

                case NODE_MOD:
                    if (rhs->data.value == 0) {
                        struct AstNode *opt_expr = ast_create_binary(buffer, expr->type, lhs, rhs);

                        if (opt_expr == NULL) {
                            return NULL;
                        }

                        return opt_expr;
                    }
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

                case NODE_LSHIFT:
                    lhs->data.value <<= rhs->data.value;
                    break;

                case NODE_RSHIFT:
                    lhs->data.value >>= rhs->data.value;
                    break;

                default:
                    assert(false);
                    errno = EINVAL;
                    return NULL;
            }

            return lhs;
        } else {
            if (expr->type == NODE_AND || expr->type == NODE_OR) {
                struct AstNode *tmp;
                if (lhs->type == NODE_NOT && lhs->data.child->type == NODE_NOT) {
                    tmp = lhs->data.child;
                    lhs->data.child = NULL;
                    lhs = tmp;
                }

                if (rhs->type == NODE_NOT && rhs->data.child->type == NODE_NOT) {
                    tmp = rhs->data.child;
                    rhs->data.child = NULL;
                    rhs = tmp;
                }
            }

            int bit_shift;
            if (
                (
                    expr->type == NODE_ADD ||
                    expr->type == NODE_SUB ||
                    expr->type == NODE_BIT_OR ||
                    expr->type == NODE_LSHIFT ||
                    expr->type == NODE_RSHIFT
                ) && rhs->type == NODE_INT && rhs->data.value == 0
            ) {
                return lhs;
            } else if (
                (
                    expr->type == NODE_ADD ||
                    expr->type == NODE_BIT_OR
                ) && lhs->type == NODE_INT && lhs->data.value == 0
            ) {
                return rhs;
            } else if (
                expr->type == NODE_OR && rhs->type == NODE_INT && rhs->data.value == 0
            ) {
                return ast_create_bool(buffer, lhs);
            } else if (
                expr->type == NODE_OR && lhs->type == NODE_INT && lhs->data.value == 0
            ) {
                return ast_create_bool(buffer, rhs);
            } else if (
                expr->type == NODE_OR && (
                    (rhs->type == NODE_INT && rhs->data.value != 0) ||
                    (lhs->type == NODE_INT && lhs->data.value != 0)
                )
            ) {
                return ast_create_int(buffer, 1);
            } else if (
                expr->type == NODE_AND && rhs->type == NODE_INT && rhs->data.value != 0
            ) {
                return ast_create_bool(buffer, lhs);
            } else if (
                expr->type == NODE_AND && lhs->type == NODE_INT && lhs->data.value != 0
            ) {
                return ast_create_bool(buffer, rhs);
            } else if (
                expr->type == NODE_SUB && lhs->type == NODE_INT && lhs->data.value == 0
            ) {
                struct AstNode *opt_expr = ast_create_unary(buffer, NODE_NEG, rhs);
                if (opt_expr == NULL) {
                    return NULL;
                }
                return opt_expr;
            } else if (
                ((
                    expr->type == NODE_MUL ||
                    expr->type == NODE_AND
                ) && (
                    (lhs->type == NODE_INT && lhs->data.value == 0) ||
                    (rhs->type == NODE_INT && rhs->data.value == 0)
                )) ||
                ((
                    expr->type == NODE_DIV ||
                    expr->type == NODE_MOD
                ) && lhs->type == NODE_INT && lhs->data.value == 0)
            ) {
                return ast_create_int(buffer, 0);
            } else if (
                expr->type == NODE_EQ && lhs->type == NODE_INT && lhs->data.value == 0
            ) {
                struct AstNode *opt_expr = ast_create_unary(buffer, NODE_NOT, rhs);
                if (opt_expr == NULL) {
                    return NULL;
                }
                return opt_expr;
            } else if (
                expr->type == NODE_EQ && rhs->type == NODE_INT && rhs->data.value == 0
            ) {
                struct AstNode *opt_expr = ast_create_unary(buffer, NODE_NOT, lhs);
                if (opt_expr == NULL) {
                    return NULL;
                }
                return opt_expr;
            } else if (
                (expr->type == NODE_MUL || expr->type == NODE_DIV) && rhs->type == NODE_INT && rhs->data.value == 1
            ) {
                return lhs;
            } else if (
                expr->type == NODE_MUL && lhs->type == NODE_INT && lhs->data.value == 1
            ) {
                return rhs;
            } else if (
                expr->type == NODE_MUL && rhs->type == NODE_INT && (bit_shift = factor_to_shift_count(rhs->data.value)) > 0
            ) {
                rhs->data.value = bit_shift;
                struct AstNode *opt_expr = ast_create_binary(buffer, NODE_LSHIFT, lhs, rhs);

                if (opt_expr == NULL) {
                    return NULL;
                }

                return opt_expr;
            } else if (
                expr->type == NODE_MUL && lhs->type == NODE_INT && (bit_shift = factor_to_shift_count(lhs->data.value)) > 0
            ) {
                lhs->data.value = bit_shift;
                struct AstNode *opt_expr = ast_create_binary(buffer, NODE_LSHIFT, rhs, lhs);

                if (opt_expr == NULL) {
                    return NULL;
                }

                return opt_expr;
            } else if (
                expr->type == NODE_DIV && rhs->type == NODE_INT && (bit_shift = factor_to_shift_count(rhs->data.value)) > 0
            ) {
                rhs->data.value = bit_shift;
                struct AstNode *opt_expr = ast_create_binary(buffer, NODE_RSHIFT, lhs, rhs);

                if (opt_expr == NULL) {
                    return NULL;
                }

                return opt_expr;
            } else {
                struct AstNode *opt_expr = ast_create_binary(buffer, expr->type, lhs, rhs);

                if (opt_expr == NULL) {
                    return NULL;
                }

                return opt_expr;
            }
        }
    } else if (expr->type == NODE_IF) {
        struct AstNode *cond_expr = ast_optimize(buffer, expr->data.terneary.cond);

        if (cond_expr == NULL) {
            return NULL;
        }

        if (cond_expr->type == NODE_INT) {
            int cond_value = cond_expr->data.value;
            if (cond_value) {
                return ast_optimize(buffer, expr->data.terneary.then_expr);
            } else {
                return ast_optimize(buffer, expr->data.terneary.else_expr);
            }
        }

        struct AstNode *then_expr = ast_optimize(buffer, expr->data.terneary.then_expr);

        if (then_expr == NULL) {
            return NULL;
        }

        struct AstNode *else_expr = ast_optimize(buffer, expr->data.terneary.else_expr);

        if (else_expr == NULL) {
            return NULL;
        }

        if (cond_expr->type == NODE_NE) {
            if (
                cond_expr->data.binary.lhs->type == NODE_INT && cond_expr->data.binary.lhs->data.value == 0
            ) {
                struct AstNode *rhs = cond_expr->data.binary.rhs;
                cond_expr->data.binary.rhs = NULL;
                cond_expr = rhs;
            } else if (
                cond_expr->data.binary.rhs->type == NODE_INT && cond_expr->data.binary.rhs->data.value == 0
            ) {
                struct AstNode *lhs = cond_expr->data.binary.lhs;
                cond_expr->data.binary.lhs = NULL;
                cond_expr = lhs;
            }
        } else if (cond_expr->type == NODE_NOT) {
            if (cond_expr->data.child->type == NODE_NOT) {
                struct AstNode *tmp = cond_expr->data.child->data.child;
                cond_expr->data.child->data.child = NULL;
                cond_expr = tmp;
            } else {
                struct AstNode *tmp = cond_expr->data.child;
                cond_expr->data.child = NULL;
                cond_expr = tmp;
                tmp = then_expr;
                then_expr = else_expr;
                else_expr = tmp;
            }
        }

        struct AstNode *if_expr = ast_create_terneary(buffer, cond_expr, then_expr, else_expr);

        if (if_expr == NULL) {
            return NULL;
        }

        return if_expr;
    } else if (ast_is_unary(expr)) {
        struct AstNode *child = ast_optimize(buffer, expr->data.child);

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
            return new_expr;
        }

        struct AstNode *unary_expr = ast_create_unary(buffer, expr->type, child);

        if (unary_expr == NULL) {
            return NULL;
        }

        return unary_expr;
    } else if (expr->type == NODE_INT) {
        return ast_create_int(buffer, expr->data.value);
    } else if (expr->type == NODE_VAR) {
        char *name = strdup(expr->data.ident);
        if (name == NULL) {
            return NULL;
        }
        return ast_create_var(buffer, name);
    } else {
        assert(false);
        errno = EINVAL;
        return NULL;
    }
}
