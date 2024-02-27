#include "alt_parser.h"

#include <errno.h>
#include <assert.h>
#include <stdbool.h>

static inline int get_precedence(enum NodeType type);
static inline bool is_binary_operation(enum TokenType token);
static inline enum NodeType get_binary_node_type(enum TokenType token);

// This parser is not a simple 1:1 translation from the BNF, but it is a tiny
// bit faster, has less redundant code, and is more flexible in regards of
// changing operator precedence.
// If you want to change the precendence of operators in the parser above you
// need to restructure the BNF and the matching calls in the parser.
// This parser is faster by having fewer recursions. The difference gets bigger
// when the grammar gets more complex since the parser above will gain a
// recursion level for every new operator, but this will stay at the same level.
// This parser uses the same tokenizer as the parser above.

struct AstNode *alt_parse_expression_from_string(const char *input) {
    struct Tokenizer tokenizer = TOKENIZER_INIT(input);
    struct AstNode *expr = alt_parse_expression(&tokenizer, 0);
    if (expr == NULL) {
        return NULL;
    }

    if (next_token(&tokenizer) != TOK_EOF) {
        // trailing garbage
        ast_free(expr);
        expr = NULL;
        if (tokenizer.token != TOK_ERROR) {
            // otherwise next_token() has set errno
            errno = EINVAL;
        }
    }
    tokenizer_free(&tokenizer);
    return expr;
}


struct AstNode *alt_parse_increasing_precedence(struct Tokenizer *tokenizer, struct AstNode *left, int min_precedence) {
    enum TokenType token = peek_token(tokenizer);

    if (!is_binary_operation(token)) {
        return left;
    }

    enum NodeType type = get_binary_node_type(token);
    int precedence = get_precedence(type);

    if (precedence <= min_precedence) {
        return left;
    }

    // eat the peeked token
    next_token(tokenizer);

    struct AstNode *right = alt_parse_expression(tokenizer, precedence);
    if (right == NULL) {
        ast_free(left);
        return NULL;
    }

    struct AstNode *expr = ast_create_binary(type, left, right);
    if (expr == NULL) {
        ast_free(left);
        ast_free(right);
        return NULL;
    }

    return expr;
}

struct AstNode *alt_parse_expression(struct Tokenizer *tokenizer, int min_precedence) {
    struct AstNode *expr = alt_parse_leaf(tokenizer);
    if (expr == NULL) {
        return NULL;
    }

    for (;;) {
        struct AstNode *new_expr = alt_parse_increasing_precedence(tokenizer, expr, min_precedence);
        if (new_expr == NULL) {
            return NULL;
        }

        if (new_expr == expr) {
            break;
        }

        expr = new_expr;
    }

    return expr;
}

struct AstNode *alt_parse_leaf(struct Tokenizer *tokenizer) {
    enum TokenType token = next_token(tokenizer);
    bool negate = false;
    struct AstNode *expr = NULL;

    for (;;) {
        if (token == TOK_MINUS) {
            negate = !negate;
        } else if (token != TOK_PLUS) {
            break;
        }
        token = next_token(tokenizer);
    }

    switch (token) {
        case TOK_INT:
            expr = ast_create_int(tokenizer->value);
            if (expr == NULL) {
                return NULL;
            }
            break;

        case TOK_IDENT:
            // re-use memory allocated for tokenizer->ident
            expr = ast_create_var(tokenizer->ident);
            if (expr == NULL) {
                return NULL;
            }
            tokenizer->ident = NULL;
            break;

        case TOK_OPEN_PAREN:
            expr = alt_parse_expression(tokenizer, 0);
            if (expr == NULL) {
                return NULL;
            }

            if (next_token(tokenizer) != TOK_CLOSE_PAREN) {
                if (tokenizer->token != TOK_ERROR) {
                    errno = EINVAL;
                }
                ast_free(expr);
                return NULL;
            }
            break;

        default:
            errno = EINVAL;
            return NULL;
    }

    if (negate) {
        struct AstNode *new_expr = ast_create_unary(NODE_NEG, expr);
        if (new_expr == NULL) {
            ast_free(expr);
            return NULL;
        }
        expr = new_expr;
    }

    return expr;
}

bool is_binary_operation(enum TokenType token) {
    switch (token) {
        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_MUL:
        case TOK_DIV:
            return true;

        default:
            return false;
    }
}

int get_precedence(enum NodeType type) {
    switch (type) {
        case NODE_ADD:
        case NODE_SUB:
            return 1;

        case NODE_MUL:
        case NODE_DIV:
            return 2;

        case NODE_NEG:
            return 3;

        case NODE_VAR:
        case NODE_INT:
            return 4;

        default:
            assert(false);
            return -1;
    }
}

enum NodeType get_binary_node_type(enum TokenType token) {
    switch (token) {
        case TOK_PLUS:  return NODE_ADD;
        case TOK_MINUS: return NODE_SUB;
        case TOK_MUL:   return NODE_MUL;
        case TOK_DIV:   return NODE_DIV;

        default:
            assert(false);
            return -1;
    }
}
