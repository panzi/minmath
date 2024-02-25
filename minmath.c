#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>

// The "minmath" langauge
// ----------------------
//
// # Grammar
// EXPRESSION := SUM
// SUM        := PRODUCT (("+" | "-") PRODUCT)*
// PRODUCT    := UNARY (("*" | "/") UNARY)*
// UNARY      := "-"* ATOM
// ATOM       := VARIABLE | INTEGER | "(" EXPRESSION ")"
// VARIABLE   := ("a" ... "z" | "a" ... "z" | "_") ("a" ... "z" | "a" ... "z" | "_" | "0" ... "9")*
// INTEGER    := ("+" | "-")? ("0" ... "9")+
//
// COMMENT    := "#" NOT_NEWLINE* ("\n" | EOF)
//
// # Other Tokens
// PLUS        := "+"
// MINUS       := "-"
// MUL         := "*"
// DIV         := "/"
// OPEN_PAREN  := "("
// CLOSE_PAREN := ")"

enum TokenType {
    TOK_START = 0,
    TOK_EOF   = -1,
    TOK_ERROR = -2,
    TOK_PLUS  = '+',
    TOK_MINUS = '-',
    TOK_MUL   = '*',
    TOK_DIV   = '/',
    TOK_INT   = '0',
    TOK_IDENT = 'a',
    TOK_OPEN_PAREN  = '(',
    TOK_CLOSE_PAREN = ')',
};

enum NodeType {
    NODE_ADD,
    NODE_SUB,
    NODE_MUL,
    NODE_DIV,
    NODE_NEG,
    NODE_VAR,
    NODE_INT,
};

struct Tokenizer {
    const char *input;
    size_t input_pos;

    enum TokenType token;
    bool peeked;
    int value;
    char *ident;
};

#define TOKENIZER_INIT(INPUT) { \
    .input = (INPUT), \
    .input_pos = 0, \
    .token = TOK_START, \
    .peeked = false, \
    .value = -1, \
    .ident = NULL, \
}

struct AstNode {
    enum NodeType type;
    union {
        int value;
        char *ident;
        struct AstNode *child;
        struct {
            struct AstNode *lhs;
            struct AstNode *rhs;
        } children;
    } data;
};

enum TokenType peek_token(struct Tokenizer *tokenizer);
enum TokenType next_token(struct Tokenizer *tokenizer);
void tokenizer_free(struct Tokenizer *tokenizer);

struct AstNode *parse_expression_from_string(const char *input);
struct AstNode *parse_expression(struct Tokenizer *tokenizer);
struct AstNode *parse_sum(struct Tokenizer *tokenizer);
struct AstNode *parse_product(struct Tokenizer *tokenizer);
struct AstNode *parse_unary(struct Tokenizer *tokenizer);
struct AstNode *parse_atom(struct Tokenizer *tokenizer);

struct AstNode *ast_create_binary(enum NodeType type, struct AstNode *lhs, struct AstNode *rhs);
struct AstNode *ast_create_unary(enum NodeType type, struct AstNode *child);
struct AstNode *ast_create_int(int value);
struct AstNode *ast_create_var(char *name);
void ast_free(struct AstNode *node);
int ast_execute(struct AstNode *expr);

void tokenizer_free(struct Tokenizer *tokenizer) {
    tokenizer->input = NULL;
    tokenizer->input_pos = 0;
    tokenizer->token = TOK_EOF;
    tokenizer->value = -1;
    if (tokenizer->ident != NULL) {
        free(tokenizer->ident);
        tokenizer->ident = NULL;
    }
}

enum TokenType peek_token(struct Tokenizer *tokenizer) {
    if (!tokenizer->peeked) {
        next_token(tokenizer);
        tokenizer->peeked = true;
    }
    return tokenizer->token;
}

enum TokenType next_token(struct Tokenizer *tokenizer) {
    if (tokenizer->peeked) {
        tokenizer->peeked = false;
        return tokenizer->token;
    }

    if (tokenizer->ident != NULL) {
        free(tokenizer->ident);
        tokenizer->ident = NULL;
    }

    char ch = tokenizer->input[tokenizer->input_pos];

    // skip whitespace and comments
    for (;;) {
        // skip whitespace
        while (isspace(ch)) {
            tokenizer->input_pos ++;
            ch = tokenizer->input[tokenizer->input_pos];
        }

        if (ch == 0) {
            return tokenizer->token = TOK_EOF;
        }

        if (ch != '#') {
            break;
        }

        // skip comment
        tokenizer->input_pos ++;
        ch = tokenizer->input[tokenizer->input_pos];
        while (ch != '\n' && ch != 0) {
            tokenizer->input_pos ++;
            ch = tokenizer->input[tokenizer->input_pos];
        }
    }

    switch (ch) {
        case '+':
        case '-':
        {
            char op = ch;
            tokenizer->input_pos ++;
            ch = tokenizer->input[tokenizer->input_pos];
            if (ch >= '0' && ch <= '9') {
                int value = 0;
                while (ch >= '0' && ch <= '9') {
                    value *= 10;
                    value += ch - '0';
                    tokenizer->input_pos ++;
                    ch = tokenizer->input[tokenizer->input_pos];
                }
                if (op == '-') {
                    value = -value;
                }
                tokenizer->value = value;
                return tokenizer->token = TOK_INT;
            }
            return tokenizer->token = (enum TokenType) op;
        }
        case '*':
        case '/':
        case '(':
        case ')':
            tokenizer->input_pos ++;
            return tokenizer->token = (enum TokenType) ch;

        default:
            if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_') {
                size_t start_pos = tokenizer->input_pos;
                do {
                    tokenizer->input_pos ++;
                    ch = tokenizer->input[tokenizer->input_pos];
                } while ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' || (ch >= '0' && ch <= '9'));

                size_t len = tokenizer->input_pos - start_pos;
                char *ident = malloc(len + 1);
                if (ident == NULL) {
                    // malloc sets errno
                    return tokenizer->token = TOK_ERROR;
                }

                memcpy(ident, tokenizer->input + start_pos, len);
                ident[len] = 0;

                tokenizer->ident = ident;
                return tokenizer->token = TOK_IDENT;
            } else if (ch >= '0' && ch <= '9') {
                int value = ch - '0';
                tokenizer->input_pos ++;
                ch = tokenizer->input[tokenizer->input_pos];
                while (ch >= '0' && ch <= '9') {
                    value *= 10;
                    value += ch - '0';
                    tokenizer->input_pos ++;
                    ch = tokenizer->input[tokenizer->input_pos];
                }
                tokenizer->value = value;
                return tokenizer->token = TOK_INT;
            } else {
                errno = EINVAL;
                return tokenizer->token = TOK_ERROR;
            }
    }
}

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

    while (peek_token(tokenizer) == TOK_MINUS) {
        ++ neg_count;
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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <EXPRESSION>...\n", argc > 0 ? argv[0] : "minmath");
        return 1;
    }
    int status = 0;
    for (int argind = 1; argind < argc; ++ argind) {
        struct AstNode *expr = parse_expression_from_string(argv[argind]);
        if (expr != NULL) {
            int value = ast_execute(expr);
            printf("%s = %d\n", argv[argind], value);
            ast_free(expr);
        } else {
            fprintf(stderr, "%s = ERROR: %s\n", argv[argind], strerror(errno));
            status = 1;
        }
    }
    return status;
}
