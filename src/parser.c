#include "parser.h"

#include <stdlib.h>

static struct AstNode *parse_condition(struct Parser *parser);
static struct AstNode *parse_or(struct Parser *parser);
static struct AstNode *parse_and(struct Parser *parser);
static struct AstNode *parse_compare(struct Parser *parser);
static struct AstNode *parse_order(struct Parser *parser);
static struct AstNode *parse_bit_shift(struct Parser *parser);
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

struct AstNode *parse(const char *input, struct ErrorInfo *error) {
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

    if (error != NULL) {
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

    for (;;) {
        enum TokenType token = peek_token(&parser->tokenizer);
        if (token != TOK_QUEST) {
            break;
        }

        token = next_token(&parser->tokenizer);

        size_t quest_offset = parser->tokenizer.token_pos;
        struct AstNode *then_expr = parse_expression(parser);
        if (then_expr == NULL) {
            ast_free(expr);
            return NULL;
        }

        token = next_token(&parser->tokenizer);

        if (token != TOK_COLON) {
            parser->error.error = PARSER_ERROR_EXPECTED_TOKEN;
            parser->error.token = TOK_COLON;
            parser->error.offset = parser->tokenizer.token_pos;
            parser->error.context_offset = quest_offset;
            ast_free(expr);
            ast_free(then_expr);
            return NULL;
        }

        struct AstNode *else_expr = parse_expression(parser);
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

        expr = if_expr;
    }

    return expr;
}

struct AstNode *parse_or(struct Parser *parser) {
    struct AstNode *expr = parse_and(parser);
    if (expr == NULL) {
        return NULL;
    }

    for (;;) {
        enum TokenType token = peek_token(&parser->tokenizer);
        if (token != TOK_OR) {
            break;
        }

        token = next_token(&parser->tokenizer);
        struct AstNode *rhs = parse_and(parser);
        if (rhs == NULL) {
            ast_free(expr);
            return NULL;
        }

        struct AstNode *new_expr = ast_create_binary(NODE_OR, expr, rhs);
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

struct AstNode *parse_and(struct Parser *parser) {
    struct AstNode *expr = parse_bit_or(parser);
    if (expr == NULL) {
        return NULL;
    }

    for (;;) {
        enum TokenType token = peek_token(&parser->tokenizer);
        if (token != TOK_AND) {
            break;
        }

        token = next_token(&parser->tokenizer);
        struct AstNode *rhs = parse_bit_or(parser);
        if (rhs == NULL) {
            ast_free(expr);
            return NULL;
        }

        struct AstNode *new_expr = ast_create_binary(NODE_AND, expr, rhs);
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

struct AstNode *parse_bit_or(struct Parser *parser) {
    struct AstNode *expr = parse_bit_xor(parser);
    if (expr == NULL) {
        return NULL;
    }

    for (;;) {
        enum TokenType token = peek_token(&parser->tokenizer);
        if (token != TOK_BIT_OR) {
            break;
        }

        token = next_token(&parser->tokenizer);
        struct AstNode *rhs = parse_bit_xor(parser);
        if (rhs == NULL) {
            ast_free(expr);
            return NULL;
        }

        struct AstNode *new_expr = ast_create_binary(NODE_BIT_OR, expr, rhs);
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

struct AstNode *parse_bit_xor(struct Parser *parser) {
    struct AstNode *expr = parse_bit_and(parser);
    if (expr == NULL) {
        return NULL;
    }

    for (;;) {
        enum TokenType token = peek_token(&parser->tokenizer);
        if (token != TOK_BIT_XOR) {
            break;
        }

        token = next_token(&parser->tokenizer);
        struct AstNode *rhs = parse_bit_and(parser);
        if (rhs == NULL) {
            ast_free(expr);
            return NULL;
        }

        struct AstNode *new_expr = ast_create_binary(NODE_BIT_XOR, expr, rhs);
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

struct AstNode *parse_bit_and(struct Parser *parser) {
    struct AstNode *expr = parse_compare(parser);
    if (expr == NULL) {
        return NULL;
    }

    for (;;) {
        enum TokenType token = peek_token(&parser->tokenizer);
        if (token != TOK_BIT_AND) {
            break;
        }

        token = next_token(&parser->tokenizer);
        struct AstNode *rhs = parse_compare(parser);
        if (rhs == NULL) {
            ast_free(expr);
            return NULL;
        }

        struct AstNode *new_expr = ast_create_binary(NODE_BIT_AND, expr, rhs);
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

struct AstNode *parse_compare(struct Parser *parser) {
    struct AstNode *expr = parse_order(parser);
    if (expr == NULL) {
        return NULL;
    }

    for (;;) {
        enum TokenType token = peek_token(&parser->tokenizer);
        enum NodeType node_type;
        switch (token) {
            case TOK_EQ: node_type = NODE_EQ; break;
            case TOK_NE: node_type = NODE_NE; break;
            default:
                return expr;
        }

        token = next_token(&parser->tokenizer);
        struct AstNode *rhs = parse_order(parser);
        if (rhs == NULL) {
            ast_free(expr);
            return NULL;
        }

        struct AstNode *new_expr = ast_create_binary(node_type, expr, rhs);
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

struct AstNode *parse_order(struct Parser *parser) {
    struct AstNode *expr = parse_bit_shift(parser);
    if (expr == NULL) {
        return NULL;
    }

    for (;;) {
        enum TokenType token = peek_token(&parser->tokenizer);
        enum NodeType node_type;
        switch (token) {
            case TOK_LT: node_type = NODE_LT; break;
            case TOK_GT: node_type = NODE_GT; break;
            case TOK_LE: node_type = NODE_LE; break;
            case TOK_GE: node_type = NODE_GE; break;
            default:
                return expr;
        }

        token = next_token(&parser->tokenizer);
        struct AstNode *rhs = parse_bit_shift(parser);
        if (rhs == NULL) {
            ast_free(expr);
            return NULL;
        }

        struct AstNode *new_expr = ast_create_binary(node_type, expr, rhs);
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

struct AstNode *parse_bit_shift(struct Parser *parser) {
    struct AstNode *expr = parse_sum(parser);
    if (expr == NULL) {
        return NULL;
    }

    for (;;) {
        enum TokenType token = peek_token(&parser->tokenizer);
        enum NodeType node_type;
        switch (token) {
            case TOK_LSHIFT: node_type = NODE_LSHIFT; break;
            case TOK_RSHIFT: node_type = NODE_RSHIFT; break;
            default:
                return expr;
        }

        token = next_token(&parser->tokenizer);
        struct AstNode *rhs = parse_sum(parser);
        if (rhs == NULL) {
            ast_free(expr);
            return NULL;
        }

        struct AstNode *new_expr = ast_create_binary(node_type, expr, rhs);
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

struct AstNode *parse_sum(struct Parser *parser) {
    struct AstNode *expr = parse_product(parser);
    if (expr == NULL) {
        return NULL;
    }

    for (;;) {
        enum TokenType token = peek_token(&parser->tokenizer);
        enum NodeType node_type;
        switch (token) {
            case TOK_PLUS:  node_type = NODE_ADD; break;
            case TOK_MINUS: node_type = NODE_SUB; break;
            default:
                return expr;
        }

        token = next_token(&parser->tokenizer);
        struct AstNode *rhs = parse_product(parser);
        if (rhs == NULL) {
            ast_free(expr);
            return NULL;
        }

        struct AstNode *new_expr = ast_create_binary(node_type, expr, rhs);
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
        enum NodeType node_type;
        switch (token) {
            case TOK_MUL: node_type = NODE_MUL; break;
            case TOK_DIV: node_type = NODE_DIV; break;
            case TOK_MOD: node_type = NODE_MOD; break;
            default:
                return expr;
        }

        token = next_token(&parser->tokenizer);
        struct AstNode *rhs = parse_unary(parser);
        if (rhs == NULL) {
            ast_free(expr);
            return NULL;
        }

        struct AstNode *new_expr = ast_create_binary(node_type, expr, rhs);
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
    enum TokenType token = peek_token(&parser->tokenizer);
    struct AstNode *top = NULL;
    struct AstNode *parent = NULL;

    for (;;) {
        if (token == TOK_PLUS) {
            next_token(&parser->tokenizer);
            token = peek_token(&parser->tokenizer);
            continue;
        }

        struct AstNode *child;
        if (token == TOK_MINUS) {
            child = ast_create_unary(NODE_NEG, NULL);
        } else if (token == TOK_BIT_NEG) {
            child = ast_create_unary(NODE_BIT_NEG, NULL);
        } else if (token == TOK_NOT) {
            child = ast_create_unary(NODE_NOT, NULL);
        } else {
            break;
        }

        if (child == NULL) {
            parser->error.error  = PARSER_ERROR_MEMORY;
            parser->error.offset = parser->error.context_offset = parser->tokenizer.token_pos;
            ast_free(top);
            return NULL;
        }

        if (parent != NULL) {
            parent->data.child = child;
            parent = child;
        } else {
            top = parent = child;
        }

        next_token(&parser->tokenizer);
        token = peek_token(&parser->tokenizer);
    }

    struct AstNode *child = parse_atom(parser);
    if (child == NULL) {
        ast_free(top);
        return NULL;
    }

    if (parent != NULL) {
        parent->data.child = child;
        return top;
    }

    return child;
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
            char *name = tokenizer_get_ident(&parser->tokenizer);
            if (name == NULL) {
                parser->error.error  = PARSER_ERROR_MEMORY;
                parser->error.offset = parser->error.context_offset = parser->tokenizer.token_pos;
                return NULL;
            }
            struct AstNode *expr = ast_create_var(name);
            if (expr == NULL) {
                free(name);
                parser->error.error  = PARSER_ERROR_MEMORY;
                parser->error.offset = parser->error.context_offset = parser->tokenizer.token_pos;
                return NULL;
            }
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
                    parser->error.error  = PARSER_ERROR_EXPECTED_TOKEN;
                    parser->error.token  = TOK_RPAREN;
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
