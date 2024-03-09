#include "fast_parser.h"

#include <assert.h>
#include <stdbool.h>

static struct AstNode *fast_parse_expression(struct FastParser *parser, int min_precedence);
static struct AstNode *fast_parse_increasing_precedence(struct FastParser *parser, struct AstNode *left, int min_precedence);
static struct AstNode *fast_parse_leaf(struct FastParser *parser);
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

void fast_parser_free(struct FastParser *parser) {
    tokenizer_free(&parser->tokenizer);
}

struct AstNode *fast_parse(const char *input, struct ErrorInfo *error) {
    struct FastParser parser = FAST_PARSER_INIT(input);
    struct AstNode *expr = fast_parse_expression(&parser, 0);
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

    fast_parser_free(&parser);
    return expr;
}


struct AstNode *fast_parse_increasing_precedence(struct FastParser *parser, struct AstNode *left, int min_precedence) {
    enum TokenType token = peek_token(&parser->tokenizer);

    if (token == TOK_QUEST) {
        if (get_precedence(NODE_IF) <= min_precedence) {
            return left;
        }
        next_token(&parser->tokenizer);

        size_t start_offset = parser->tokenizer.token_pos;
        struct AstNode *then_expr = fast_parse_expression(parser, 0);
        if (then_expr == NULL) {
            ast_free(left);
            return NULL;
        }

        if (next_token(&parser->tokenizer) != TOK_COLON) {
            parser->error.error  = PARSER_ERROR_ILLEGAL_TOKEN;
            parser->error.offset = parser->tokenizer.token_pos;
            parser->error.context_offset = start_offset;
            ast_free(left);
            return NULL;
        }

        struct AstNode *else_expr = fast_parse_expression(parser, 0);
        if (else_expr == NULL) {
            ast_free(left);
            ast_free(then_expr);
            return NULL;
        }

        struct AstNode *if_expr = ast_create_terneary(left, then_expr, else_expr);
        if (if_expr == NULL) {
            parser->error.error  = PARSER_ERROR_MEMORY;
            parser->error.offset = parser->error.context_offset = parser->tokenizer.token_pos;
            ast_free(left);
            ast_free(then_expr);
            ast_free(else_expr);
            return NULL;
        }

        return if_expr;
    }

    if (!is_binary_operation(token)) {
        return left;
    }

    enum NodeType type = get_binary_node_type(token);
    int precedence = get_precedence(type);

    if (precedence <= min_precedence) {
        return left;
    }

    // eat the peeked token
    next_token(&parser->tokenizer);

    struct AstNode *right = fast_parse_expression(parser, precedence);
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

struct AstNode *fast_parse_expression(struct FastParser *parser, int min_precedence) {
    struct AstNode *expr = fast_parse_leaf(parser);
    if (expr == NULL) {
        return NULL;
    }

    for (;;) {
        struct AstNode *new_expr = fast_parse_increasing_precedence(parser, expr, min_precedence);
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

struct AstNode *fast_parse_leaf(struct FastParser *parser) {
    enum TokenType token = next_token(&parser->tokenizer);
    struct AstNode *top = NULL;
    struct AstNode *parent = NULL;

    for (;;) {
        if (token == TOK_PLUS) {
            token = next_token(&parser->tokenizer);
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

        token = next_token(&parser->tokenizer);
    }

    struct AstNode *child = NULL;
    switch (token) {
        case TOK_INT:
            child = ast_create_int(parser->tokenizer.value);
            if (child == NULL) {
                ast_free(top);
                parser->error.error  = PARSER_ERROR_MEMORY;
                parser->error.offset = parser->error.context_offset = parser->tokenizer.token_pos;
                return NULL;
            }
            break;

        case TOK_IDENT:
            // re-use memory allocated for tokenizer->ident
            child = ast_create_var(parser->tokenizer.ident);
            if (child == NULL) {
                ast_free(top);
                parser->error.error  = PARSER_ERROR_MEMORY;
                parser->error.offset = parser->error.context_offset = parser->tokenizer.token_pos;
                return NULL;
            }
            parser->tokenizer.ident = NULL;
            break;

        case TOK_LPAREN:
        {
            size_t start_offset = parser->tokenizer.token_pos;
            child = fast_parse_expression(parser, 0);
            if (child == NULL) {
                ast_free(top);
                return NULL;
            }

            if (next_token(&parser->tokenizer) != TOK_RPAREN) {
                if (!token_is_error(parser->tokenizer.token)) {
                    parser->error.error  = PARSER_ERROR_EXPECTED_TOKEN;
                    parser->error.token  = TOK_RPAREN;
                    parser->error.offset = parser->tokenizer.token_pos;
                    parser->error.context_offset = start_offset;
                }
                ast_free(child);
                ast_free(top);
                return NULL;
            }
            break;
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

    if (parent != NULL) {
        parent->data.child = child;
        return top;
    }

    return child;
}

bool is_binary_operation(enum TokenType token) {
    switch (token) {
        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_MUL:
        case TOK_DIV:
        case TOK_MOD:
        case TOK_BIT_OR:
        case TOK_BIT_XOR:
        case TOK_BIT_AND:
        case TOK_AND:
        case TOK_OR:
        case TOK_LT:
        case TOK_GT:
        case TOK_LE:
        case TOK_GE:
        case TOK_EQ:
        case TOK_NE:
        case TOK_LSHIFT:
        case TOK_RSHIFT:
            return true;

        default:
            return false;
    }
}

int get_precedence(enum NodeType type) {
    switch (type) {
        case NODE_IF:
            return 1;

        case NODE_OR:
            return 2;

        case NODE_AND:
            return 3;

        case NODE_BIT_OR:
            return 4;

        case NODE_BIT_XOR:
            return 5;

        case NODE_BIT_AND:
            return 6;

        case NODE_EQ:
        case NODE_NE:
            return 7;

        case NODE_LT:
        case NODE_GT:
        case NODE_LE:
        case NODE_GE:
            return 8;

        case NODE_LSHIFT:
        case NODE_RSHIFT:
            return 9;

        case NODE_ADD:
        case NODE_SUB:
            return 10;

        case NODE_MUL:
        case NODE_DIV:
        case NODE_MOD:
            return 11;

        case NODE_NEG:
        case NODE_BIT_NEG:
        case NODE_NOT:
            return 12;

        case NODE_VAR:
        case NODE_INT:
            return 13;

        default:
            assert(false);
            return -1;
    }
}

enum NodeType get_binary_node_type(enum TokenType token) {
    switch (token) {
        case TOK_PLUS:    return NODE_ADD;
        case TOK_MINUS:   return NODE_SUB;
        case TOK_MUL:     return NODE_MUL;
        case TOK_DIV:     return NODE_DIV;
        case TOK_MOD:     return NODE_MOD;
        case TOK_BIT_OR:  return NODE_BIT_OR;
        case TOK_BIT_XOR: return NODE_BIT_XOR;
        case TOK_BIT_AND: return NODE_BIT_AND;
        case TOK_AND:     return NODE_AND;
        case TOK_OR:      return NODE_OR;
        case TOK_LT:      return NODE_LT;
        case TOK_GT:      return NODE_GT;
        case TOK_LE:      return NODE_LE;
        case TOK_GE:      return NODE_GE;
        case TOK_EQ:      return NODE_EQ;
        case TOK_NE:      return NODE_NE;
        case TOK_LSHIFT:  return NODE_LSHIFT;
        case TOK_RSHIFT:  return NODE_RSHIFT;

        default:
            assert(false);
            return -1;
    }
}
