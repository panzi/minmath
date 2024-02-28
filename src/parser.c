#include "parser.h"

static struct AstNode *parse_condition(struct Parser *parser);
static struct AstNode *parse_or(struct Parser *parser);
static struct AstNode *parse_and(struct Parser *parser);
static struct AstNode *parse_compare(struct Parser *parser);
static struct AstNode *parse_bit_or(struct Parser *parser);
static struct AstNode *parse_bit_xor(struct Parser *parser);
static struct AstNode *parse_bit_and(struct Parser *parser);
static struct AstNode *parse_sum(struct Parser *parser);
static struct AstNode *parse_product(struct Parser *parser);
static struct AstNode *parse_unary(struct Parser *parser);
static struct AstNode *parse_atom(struct Parser *parser);

void parser_free(struct Parser *parser) {
    tokenizer_free(&parser->tokenizer);
}

struct AstNode *parse_expression_from_string(const char *input, struct ErrorInfo *error) {
    struct Parser parser = PARSER_INIT(input);
    struct AstNode *expr = parse_expression(&parser);
    if (expr != NULL) {
        if (next_token(&parser.tokenizer) != TOK_EOF) {
            parser.error.error  = PARSER_ERROR_ILLEGAL_TOKEN;
            parser.error.offset = parser.error.context_offset = parser.tokenizer.token_pos;
            ast_free(expr);
            expr = NULL;
        }
    }

    if (parser.error.error != PARSER_ERROR_OK && error != NULL) {
        *error = parser.error;
    }

    parser_free(&parser);
    return expr;
}

// The actual grammar parsing happens here:
struct AstNode *parse_expression(struct Parser *parser) {
    return parse_condition(parser);
}

struct AstNode *parse_condition(struct Parser *parser) {
    struct AstNode *expr = parse_or(parser);
    if (expr == NULL) {
        return NULL;
    }

    enum TokenType token = peek_token(&parser->tokenizer);

    if (token != TOK_QUEST) {
        return expr;
    }

    token = next_token(&parser->tokenizer);

    size_t quest_offset = parser->tokenizer.token_pos;
    struct AstNode *then_expr = parse_or(parser);
    if (then_expr == NULL) {
        ast_free(expr);
        return NULL;
    }

    token = next_token(&parser->tokenizer);

    if (token != TOK_COLON) {
        parser->error.error = PARSER_ERROR_ILLEGAL_TOKEN;
        parser->error.offset = parser->tokenizer.token_pos;
        parser->error.context_offset = quest_offset;
        ast_free(expr);
        ast_free(then_expr);
        return NULL;
    }

    struct AstNode *else_expr = parse_or(parser);
    if (else_expr == NULL) {
        ast_free(expr);
        ast_free(then_expr);
        return NULL;
    }

    struct AstNode *if_expr = ast_create_terneary(expr, then_expr, else_expr);
    if (if_expr == NULL) {
        parser->error.error  = PARSER_ERROR_MEMORY;
        parser->error.offset = parser->error.context_offset = parser->tokenizer.token_pos;
        ast_free(expr);
        ast_free(then_expr);
        ast_free(else_expr);
        return NULL;
    }

    return if_expr;
}

struct AstNode *parse_or(struct Parser *parser) {
    // TODO
}

struct AstNode *parse_and(struct Parser *parser) {
    // TODO
}

struct AstNode *parse_compare(struct Parser *parser) {
    // TODO
}

struct AstNode *parse_bit_or(struct Parser *parser) {
    // TODO
}

struct AstNode *parse_bit_xor(struct Parser *parser) {
    // TODO
}

struct AstNode *parse_bit_and(struct Parser *parser) {
    // TODO
}

struct AstNode *parse_sum(struct Parser *parser) {
    struct AstNode *expr = parse_product(parser);
    if (expr == NULL) {
        return NULL;
    }

    for (;;) {
        enum TokenType token = peek_token(&parser->tokenizer);
        if (token != TOK_PLUS && token != TOK_MINUS) {
            break;
        }

        token = next_token(&parser->tokenizer);
        struct AstNode *rhs = parse_product(parser);
        if (rhs == NULL) {
            ast_free(expr);
            return NULL;
        }

        struct AstNode *new_expr = ast_create_binary(token == TOK_PLUS ? NODE_ADD : NODE_SUB, expr, rhs);
        if (new_expr == NULL) {
            parser->error.error  = PARSER_ERROR_MEMORY;
            parser->error.offset = parser->error.context_offset = parser->tokenizer.token_pos;
            ast_free(expr);
            ast_free(rhs);
            return NULL;
        }
        expr = new_expr;
    }
    return expr;
}

struct AstNode *parse_product(struct Parser *parser) {
    struct AstNode *expr = parse_unary(parser);
    if (expr == NULL) {
        return NULL;
    }

    for (;;) {
        enum TokenType token = peek_token(&parser->tokenizer);
        if (token != TOK_MUL && token != TOK_DIV) {
            break;
        }

        token = next_token(&parser->tokenizer);
        struct AstNode *rhs = parse_unary(parser);
        if (rhs == NULL) {
            ast_free(expr);
            return NULL;
        }

        struct AstNode *new_expr = ast_create_binary(token == TOK_MUL ? NODE_MUL : NODE_DIV, expr, rhs);
        if (new_expr == NULL) {
            parser->error.error  = PARSER_ERROR_MEMORY;
            parser->error.offset = parser->error.context_offset = parser->tokenizer.token_pos;
            ast_free(expr);
            ast_free(rhs);
            return NULL;
        }
        expr = new_expr;
    }
    return expr;
}

struct AstNode *parse_unary(struct Parser *parser) {
    size_t neg_count = 0;

    for (;;) {
        enum TokenType token = peek_token(&parser->tokenizer);
        if (token == TOK_MINUS) {
            ++ neg_count;
        } else if (token != TOK_PLUS) {
            break;
        }
        next_token(&parser->tokenizer);
    }

    struct AstNode *expr = parse_atom(parser);
    if (expr == NULL) {
        return NULL;
    }

    if (neg_count % 2 == 1) {
        struct AstNode *new_expr = ast_create_unary(NODE_NEG, expr);
        if (new_expr == NULL) {
            parser->error.error  = PARSER_ERROR_MEMORY;
            parser->error.offset = parser->error.context_offset = parser->tokenizer.token_pos;
            ast_free(expr);
            return NULL;
        }
        expr = new_expr;
    }

    return expr;
}

struct AstNode *parse_atom(struct Parser *parser) {
    enum TokenType token = next_token(&parser->tokenizer);
    switch (token) {
        case TOK_INT:
        {
            struct AstNode *expr = ast_create_int(parser->tokenizer.value);
            if (expr == NULL) {
                parser->error.error  = PARSER_ERROR_MEMORY;
                parser->error.offset = parser->error.context_offset = parser->tokenizer.token_pos;
                return NULL;
            }
            return expr;
        }
        case TOK_IDENT:
        {
            // re-use memory allocated for tokenizer->ident
            struct AstNode *expr = ast_create_var(parser->tokenizer.ident);
            if (expr == NULL) {
                parser->error.error  = PARSER_ERROR_MEMORY;
                parser->error.offset = parser->error.context_offset = parser->tokenizer.token_pos;
                return NULL;
            }
            parser->tokenizer.ident = NULL;
            return expr;
        }
        case TOK_LPAREN:
        {
            size_t start_offset = parser->tokenizer.token_pos;
            struct AstNode *expr = parse_expression(parser);
            if (expr == NULL) {
                return NULL;
            }

            if (next_token(&parser->tokenizer) != TOK_RPAREN) {
                if (!token_is_error(parser->tokenizer.token)) {
                    parser->error.error  = PARSER_ERROR_EXPECTED_CLOSE_PAREN;
                    parser->error.offset = parser->tokenizer.token_pos;
                    parser->error.context_offset = start_offset;
                }
                ast_free(expr);
                return NULL;
            }
            return expr;
        }
        case TOK_EOF:
            parser->error.error  = PARSER_ERROR_UNEXPECTED_EOF;
            parser->error.offset = parser->error.context_offset = parser->tokenizer.token_pos;
            return NULL;

        default:
            parser->error.error  = PARSER_ERROR_ILLEGAL_TOKEN;
            parser->error.offset = parser->error.context_offset = parser->tokenizer.token_pos;
            return NULL;
    }
}
