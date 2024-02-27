#include "parser.h"

#include <errno.h>

struct AstNode *parse_expression_from_string(const char *input) {
    struct Tokenizer tokenizer = TOKENIZER_INIT(input);
    struct AstNode *expr = parse_expression(&tokenizer);
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

// The actual grammar parsing happens here:
struct AstNode *parse_expression(struct Tokenizer *tokenizer) {
    return parse_sum(tokenizer);
}

struct AstNode *parse_sum(struct Tokenizer *tokenizer) {
    struct AstNode *expr = parse_product(tokenizer);
    if (expr == NULL) {
        return NULL;
    }

    for (;;) {
        enum TokenType token = peek_token(tokenizer);
        if (token != TOK_PLUS && token != TOK_MINUS) {
            break;
        }

        token = next_token(tokenizer);
        struct AstNode *rhs = parse_product(tokenizer);
        if (rhs == NULL) {
            ast_free(expr);
            return NULL;
        }

        struct AstNode *new_expr = ast_create_binary(token == TOK_PLUS ? NODE_ADD : NODE_SUB, expr, rhs);
        if (new_expr == NULL) {
            ast_free(expr);
            ast_free(rhs);
            return NULL;
        }
        expr = new_expr;
    }
    return expr;
}

struct AstNode *parse_product(struct Tokenizer *tokenizer) {
    struct AstNode *expr = parse_unary(tokenizer);
    if (expr == NULL) {
        return NULL;
    }

    for (;;) {
        enum TokenType token = peek_token(tokenizer);
        if (token != TOK_MUL && token != TOK_DIV) {
            break;
        }

        token = next_token(tokenizer);
        struct AstNode *rhs = parse_unary(tokenizer);
        if (rhs == NULL) {
            ast_free(expr);
            return NULL;
        }

        struct AstNode *new_expr = ast_create_binary(token == TOK_MUL ? NODE_MUL : NODE_DIV, expr, rhs);
        if (new_expr == NULL) {
            ast_free(expr);
            ast_free(rhs);
            return NULL;
        }
        expr = new_expr;
    }
    return expr;
}

struct AstNode *parse_unary(struct Tokenizer *tokenizer) {
    size_t neg_count = 0;

    for (;;) {
        enum TokenType token = peek_token(tokenizer);
        if (token == TOK_MINUS) {
            ++ neg_count;
        } else if (token != TOK_PLUS) {
            break;
        }
        next_token(tokenizer);
    }

    struct AstNode *expr = parse_atom(tokenizer);
    if (expr == NULL) {
        return NULL;
    }

    if (neg_count % 2 == 1) {
        struct AstNode *new_expr = ast_create_unary(NODE_NEG, expr);
        if (new_expr == NULL) {
            ast_free(expr);
            return NULL;
        }
        expr = new_expr;
    }

    return expr;
}

struct AstNode *parse_atom(struct Tokenizer *tokenizer) {
    enum TokenType token = next_token(tokenizer);
    switch (token) {
        case TOK_INT:
            return ast_create_int(tokenizer->value);

        case TOK_IDENT:
        {
            // re-use memory allocated for tokenizer->ident
            struct AstNode *expr = ast_create_var(tokenizer->ident);
            if (expr == NULL) {
                return NULL;
            }
            tokenizer->ident = NULL;
            return expr;
        }
        case TOK_OPEN_PAREN:
        {
            struct AstNode *expr = parse_expression(tokenizer);
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
            return expr;
        }
        default:
            errno = EINVAL;
            return NULL;
    }
}
