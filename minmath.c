#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>

#ifdef TEST
#   include <time.h>
#   include <sys/time.h>
#endif

// The "minmath" langauge
// ----------------------
//
// # Grammar
// EXPRESSION := SUM
// SUM        := PRODUCT (("+" | "-") PRODUCT)*
// PRODUCT    := UNARY (("*" | "/") UNARY)*
// UNARY      := ("-" | "+")* ATOM
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

// ========================================================================== //
//                                                                            //
//                           Tokenizer Functions                              //
//                                                                            //
// ========================================================================== //

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
        while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\v') {
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

// ========================================================================== //
//                                                                            //
//                  Abstract Syntax Tree (AST) Functions                      //
//                                                                            //
// ========================================================================== //

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

// ========================================================================== //
//                                                                            //
//                     Simple Recursive Descent Parser                        //
//                                                                            //
// ========================================================================== //

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

// ========================================================================== //
//                                                                            //
//                      Alternative (Faster) Parser                           //
//                                                                            //
// ========================================================================== //

// This parser is not a simple 1:1 translation from the BNF, but it is a tiny
// bit faster, has less redundant code, and is more flexible in regards of
// changing operator precedence.
// If you want to change the precendence of operators in the parser above you
// need to restructure the BNF and the matching calls in the parser.
// This parser is faster by having fewer recursions. The difference gets bigger
// when the grammar gets more complex since the parser above will gain a
// recursion level for every new operator, but this will stay at the same level.
// This parser uses the same tokenizer as the parser above.

struct AstNode *alt_parse_expression_from_string(const char *input);
struct AstNode *alt_parse_expression(struct Tokenizer *tokenizer, int min_precedence);
struct AstNode *alt_parse_increasing_precedence(struct Tokenizer *tokenizer, struct AstNode *left, int min_precedence);
struct AstNode *alt_parse_leaf(struct Tokenizer *tokenizer);
inline int get_precedence(enum NodeType type);
inline bool is_binary_operation(enum TokenType token);
inline enum NodeType get_binary_node_type(enum TokenType token);

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

// ========================================================================== //
//                                                                            //
//                             Tree Interpreter                               //
//                                                                            //
// ========================================================================== //

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

#ifdef TEST

// ========================================================================== //
//                                                                            //
//                         Testing and Benchmarking                           //
//                                                                            //
// ========================================================================== //
// These are some randomly generated tests. Skip to #else for the normal program.

struct TestCase {
    const char *expr;
    bool parse_ok;
    char **environ;
    int result;
};

const struct TestCase TESTS[] = {
    { "S9", true, (char*[]){"S9=-1425181629", NULL}, -1425181629 },
    { "0", true, (char*[]){NULL}, 0 },
    { "289010127", true, (char*[]){NULL}, 289010127 },
    { "+ - -1582389005", true, (char*[]){NULL}, + - -1582389005 },
    { "-377395463", true, (char*[]){NULL}, -377395463 },
    { "-497006846", true, (char*[]){NULL}, -497006846 },
    { "(tIxW)", true, (char*[]){"tIxW=1578416547", NULL}, (1578416547) },
    { "IUK1lAY_r", true, (char*[]){"IUK1lAY_r=-355839675", NULL}, -355839675 },
    { "437178656", true, (char*[]){NULL}, 437178656 },
    { "312164169", true, (char*[]){NULL}, 312164169 },
    { "jmr", true, (char*[]){"jmr=1124606183", NULL}, 1124606183 },
    { "A", true, (char*[]){"A=-691890765", NULL}, -691890765 },
    { "1998176616", true, (char*[]){NULL}, 1998176616 },
    { "TR", true, (char*[]){"TR=1856321346", NULL}, 1856321346 },
    { "(+ cFTodCN)", true, (char*[]){"cFTodCN=957276972", NULL}, (+ 957276972) },
    { "2023549308", true, (char*[]){NULL}, 2023549308 },
    { "((- (BsTTHB_ * (u4gZwGgU) * DbLjn * - v_KrWfT * i2Hz)))", true, (char*[]){"BsTTHB_=1157617813", "DbLjn=931666424", "u4gZwGgU=0", "i2Hz=-1181807089", "v_KrWfT=373998198", NULL}, ((- (1157617813 * (0) * 931666424 * - 373998198 * -1181807089))) },
    { "(ir) / - 973521509", true, (char*[]){"ir=958498980", NULL}, (958498980) / - 973521509 },
    { "+ (-64209253)", true, (char*[]){NULL}, + (-64209253) },
    { "0", true, (char*[]){NULL}, 0 },
    { "- 1722376748 + EO1Uom7", true, (char*[]){"EO1Uom7=-1939365577", NULL}, - 1722376748 + -1939365577 },
    { "(F4NSrRUl)", true, (char*[]){"F4NSrRUl=1952637622", NULL}, (1952637622) },
    { "POs / LJg47cZCV / - eCBxdJ0J8", true, (char*[]){"POs=-1070975219", "LJg47cZCV=-1204669359", "eCBxdJ0J8=-1615837523", NULL}, -1070975219 / -1204669359 / - -1615837523 },
    { "1635451902", true, (char*[]){NULL}, 1635451902 },
    { "Yxhfyb", true, (char*[]){"Yxhfyb=1394359960", NULL}, 1394359960 },
    { "((I) / ((H200ppI)) - E_r)", true, (char*[]){"I=0", "E_r=1672475781", "H200ppI=-1869630664", NULL}, ((0) / ((-1869630664)) - 1672475781) },
    { "+ s5k", true, (char*[]){"s5k=-1353961291", NULL}, + -1353961291 },
    { "(- (0) + - 739789617)", true, (char*[]){NULL}, (- (0) + - 739789617) },
    { "oi", true, (char*[]){"oi=-820504137", NULL}, -820504137 },
    { "(zAHSq)", true, (char*[]){"zAHSq=-1847473827", NULL}, (-1847473827) },
    { "-217564527", true, (char*[]){NULL}, -217564527 },
    { "(- + wS01)", true, (char*[]){"wS01=2026222960", NULL}, (- + 2026222960) },
    { "+ eGt", true, (char*[]){"eGt=-696054784", NULL}, + -696054784 },
    { "-1526942278", true, (char*[]){NULL}, -1526942278 },
    { "gOYFc4Cp * -421089790", true, (char*[]){"gOYFc4Cp=0", NULL}, 0 * -421089790 },
    { "+ rmjOk", true, (char*[]){"rmjOk=697704627", NULL}, + 697704627 },
    { "((((-338473029) / HGTEzV)))", true, (char*[]){"HGTEzV=-462248027", NULL}, ((((-338473029) / -462248027))) },
    { "H4wiwOAcs", true, (char*[]){"H4wiwOAcs=512098727", NULL}, 512098727 },
    { "+ NVF6Pn", true, (char*[]){"NVF6Pn=-1363882487", NULL}, + -1363882487 },
    { "MvK_gVWf", true, (char*[]){"MvK_gVWf=-1169243874", NULL}, -1169243874 },
    { "-15149137", true, (char*[]){NULL}, -15149137 },
    { "(Sqt0 - -930083266)", true, (char*[]){"Sqt0=389563828", NULL}, (389563828 - -930083266) },
    { "-338780562", true, (char*[]){NULL}, -338780562 },
    { "+ Z0V", true, (char*[]){"Z0V=-265167851", NULL}, + -265167851 },
    { "qKgE + -144797141 / - 860404551", true, (char*[]){"qKgE=-1648141632", NULL}, -1648141632 + -144797141 / - 860404551 },
    { "+ GEt", true, (char*[]){"GEt=5079949", NULL}, + 5079949 },
    { "(-2099652861 / -1989597774) * - + AJ + + -797948732", true, (char*[]){"AJ=-376692512", NULL}, (-2099652861 / -1989597774) * - + -376692512 + + -797948732 },
    { "-192924122", true, (char*[]){NULL}, -192924122 },
    { "2131644938", true, (char*[]){NULL}, 2131644938 },
    { "-618158176", true, (char*[]){NULL}, -618158176 },
    { "(lEXH3)", true, (char*[]){"lEXH3=-484295410", NULL}, (-484295410) },
    { "LDx02CaT6 + vSNg0t", true, (char*[]){"LDx02CaT6=-1479710080", "vSNg0t=33781413", NULL}, -1479710080 + 33781413 },
    { "QV", true, (char*[]){"QV=-530947810", NULL}, -530947810 },
    { "w8", true, (char*[]){"w8=0", NULL}, 0 },
    { "208209312", true, (char*[]){NULL}, 208209312 },
    { "+ + (- - 2048764628) - nP5S * 1734005724", true, (char*[]){"nP5S=-783382586", NULL}, + + (- - 2048764628) - -783382586 * 1734005724 },
    { "((Mar36PLKd) * - (J552DP8dg - - C2l20JylW))", true, (char*[]){"C2l20JylW=-34379100", "J552DP8dg=1398362528", "Mar36PLKd=-1084469900", NULL}, ((-1084469900) * - (1398362528 - - -34379100)) },
    { "- xa", true, (char*[]){"xa=983877634", NULL}, - 983877634 },
    { "+ pSJuVt", true, (char*[]){"pSJuVt=-2034921664", NULL}, + -2034921664 },
    { "apij * 1965011289 * gpF6", true, (char*[]){"apij=56768680", "gpF6=-371840576", NULL}, 56768680 * 1965011289 * -371840576 },
    { "299363477", true, (char*[]){NULL}, 299363477 },
    { "+ 1198484484 + - pB / -2064888423", true, (char*[]){"pB=-471886299", NULL}, + 1198484484 + - -471886299 / -2064888423 },
    { "Mscxhug", true, (char*[]){"Mscxhug=-204057254", NULL}, -204057254 },
    { "(rWrND2CH)", true, (char*[]){"rWrND2CH=413923672", NULL}, (413923672) },
    { "-393087241", true, (char*[]){NULL}, -393087241 },
    { "- - - + - ZBv", true, (char*[]){"ZBv=-859049201", NULL}, - - - + - -859049201 },
    { "- bewFvpi", true, (char*[]){"bewFvpi=802352192", NULL}, - 802352192 },
    { "-1541692773", true, (char*[]){NULL}, -1541692773 },
    { "- 1060414092", true, (char*[]){NULL}, - 1060414092 },
    { "o5R1n7 * -1132425564", true, (char*[]){"o5R1n7=-366100012", NULL}, -366100012 * -1132425564 },
    { "(+ -570191264) + DJ9", true, (char*[]){"DJ9=-96755631", NULL}, (+ -570191264) + -96755631 },
    { "cDF4SHU3", true, (char*[]){"cDF4SHU3=1442215165", NULL}, 1442215165 },
    { "(-691244701 + + ((IS) / ((- -128084619)) + 1683009122 - 176336260) + + + wmrFClses + (0 * + + + (B) + (+ Ns4kK9)) / - (- MGREkHe) + (DmsSL))", true, (char*[]){"IS=-684921171", "MGREkHe=-256571185", "Ns4kK9=0", "DmsSL=527477426", "wmrFClses=0", "B=0", NULL}, (-691244701 + + ((-684921171) / ((- -128084619)) + 1683009122 - 176336260) + + + 0 + (0 * + + + (0) + (+ 0)) / - (- -256571185) + (527477426)) },
    { "(+ + QVWZ)", true, (char*[]){"QVWZ=-581615627", NULL}, (+ + -581615627) },
    { "(i0pD)", true, (char*[]){"i0pD=-107496991", NULL}, (-107496991) },
    { "gaNS7ly8", true, (char*[]){"gaNS7ly8=-1025127222", NULL}, -1025127222 },
    { "-169993511", true, (char*[]){NULL}, -169993511 },
    { "2073341662", true, (char*[]){NULL}, 2073341662 },
    { "uI", true, (char*[]){"uI=-1914502878", NULL}, -1914502878 },
    { "409084114", true, (char*[]){NULL}, 409084114 },
    { "-2144086158", true, (char*[]){NULL}, -2144086158 },
    { "(-780403992 * (jNQfC_sAe))", true, (char*[]){"jNQfC_sAe=1666485613", NULL}, (-780403992 * (1666485613)) },
    { "((0))", true, (char*[]){NULL}, ((0)) },
    { "+ y99QXhL", true, (char*[]){"y99QXhL=133734216", NULL}, + 133734216 },
    { "((+ ((1868791728) / (554700261)) - - - - ((X3Yp)) / (- 2013960216 + - (j9N) * _A25o * + (-600042271) + kjj)))", true, (char*[]){"kjj=-7689952", "j9N=1554368438", "X3Yp=290556364", "_A25o=0", NULL}, ((+ ((1868791728) / (554700261)) - - - - ((290556364)) / (- 2013960216 + - (1554368438) * 0 * + (-600042271) + -7689952))) },
    { "+ - 2004455858 - (-443850739) - - + ((-1612026233 + (-1330392806) + + + (- 0))) + (e)", true, (char*[]){"e=0", NULL}, + - 2004455858 - (-443850739) - - + ((-1612026233 + (-1330392806) + + + (- 0))) + (0) },
    { "-1397527042 / -2097475948", true, (char*[]){NULL}, -1397527042 / -2097475948 },
    { "-1560693965 * -470250363 - CisOG", true, (char*[]){"CisOG=-1223910551", NULL}, -1560693965 * -470250363 - -1223910551 },
    { "abKZ9u5", true, (char*[]){"abKZ9u5=0", NULL}, 0 },
    { "((2000138242))", true, (char*[]){NULL}, ((2000138242)) },
    { "VUSa0g", true, (char*[]){"VUSa0g=811778484", NULL}, 811778484 },
    { "(773658433)", true, (char*[]){NULL}, (773658433) },
    { "1798207366", true, (char*[]){NULL}, 1798207366 },
    { "f88Gl", true, (char*[]){"f88Gl=960948185", NULL}, 960948185 },
    { "921406370", true, (char*[]){NULL}, 921406370 },
    { "(f18N) / - + -260328536 + (0) + wUu + + (1913283708 + 0) * ((Z7hEPLV)) * Mc - - (-1643609591 / 182381941) + -450675351 - - EZ", true, (char*[]){"Mc=2070530086", "wUu=406196521", "f18N=684959907", "Z7hEPLV=-976533508", "EZ=812894099", NULL}, (684959907) / - + -260328536 + (0) + 406196521 + + (1913283708 + 0) * ((-976533508)) * 2070530086 - - (-1643609591 / 182381941) + -450675351 - - 812894099 },
    { "+ yqkTbi0mi * + 1612538887 + (- + 619420145 * -1799151132)", true, (char*[]){"yqkTbi0mi=-1213737852", NULL}, + -1213737852 * + 1612538887 + (- + 619420145 * -1799151132) },
    { "+ ((O9qpz))", true, (char*[]){"O9qpz=0", NULL}, + ((0)) },
    { "1351496505", true, (char*[]){NULL}, 1351496505 },
    { "+ + -1546548652", true, (char*[]){NULL}, + + -1546548652 },
    { NULL, false, NULL, -1 },
};

#define TS_TO_TV(TS) (struct timeval){ .tv_sec = (TS).tv_sec, .tv_usec = (TS).tv_nsec / 1000 }
#define ITERS 100000

extern char **environ;

struct ParseFunc {
    const char *name;
    struct AstNode *(*parse)(const char *input);
};

const struct ParseFunc PARSE_FUNCS[] = {
    { "recursive descend", parse_expression_from_string },
    { "pratt", parse_expression_from_string },
    { NULL, NULL },
};

int main(int argc, char *argv[]) {
    struct timespec ts_start, ts_end;
    struct timeval tv_start, tv_end;
    int res_start, res_end;
    int status = 0;

    for (const struct ParseFunc *func = PARSE_FUNCS; func->name; ++ func) {
        printf("Testing %s\n", func->name);
        for (const struct TestCase *test = TESTS; test->expr; ++ test) {
            struct AstNode *expr = func->parse(test->expr);
            if (expr == NULL) {
                if (test->parse_ok) {
                    fprintf(stderr, "*** [%s] Error parsing expression: %s\n", func->name, test->expr);
                    status = 1;
                }
            } else {
                if (!test->parse_ok) {
                    fprintf(stderr, "*** [%s] Expected error parsing expression, but was ok: %s\n", func->name, test->expr);
                    status = 1;
                }

                char **environ_bakup = environ;
                environ = test->environ;
                int result = ast_execute(expr);
                environ = environ_bakup;

                if (result != test->result) {
                    fprintf(stderr, "*** [%s] Result missmatch:\nEnvironment:\n", func->name);
                    for (char **ptr = test->environ; *ptr; ++ ptr) {
                        fprintf(stderr, "    %s\n", *ptr);
                    }
                    fprintf(stderr, "Expression:\n    %s\nResult:\n    %d\nExpected:\n    %d\n\n", test->expr, result, test->result);

                    status = 1;
                }

                ast_free(expr);
            }
        }
    }

    printf("Benchmarking parsing with %d iterations per expression:\n", ITERS);

    res_start = clock_gettime(CLOCK_MONOTONIC, &ts_start);
    for (const struct TestCase *test = TESTS; test->expr; ++ test) {
        for (size_t iter = 0; iter < ITERS; ++ iter) {
            struct AstNode *expr = parse_expression_from_string(test->expr);
            if (expr == NULL) {
                if (test->parse_ok) {
                    fprintf(stderr, "*** Error parsing expression: %s\n", test->expr);
                    return 1;
                }
            } else {
                ast_free(expr);
                if (!test->parse_ok) {
                    fprintf(stderr, "*** Expected error parsing expression, but was ok: %s\n", test->expr);
                    return 1;
                }
            }
        }
    }
    res_end = clock_gettime(CLOCK_MONOTONIC, &ts_end);
    assert(res_start == 0); (void)res_start;
    assert(res_end == 0); (void)res_end;

    tv_start = TS_TO_TV(ts_start);
    tv_end = TS_TO_TV(ts_end);

    struct timeval tv_bench;
    timersub(&tv_end, &tv_start, &tv_bench);

    res_start = clock_gettime(CLOCK_MONOTONIC, &ts_start);
    for (const struct TestCase *test = TESTS; test->expr; ++ test) {
        for (size_t iter = 0; iter < ITERS; ++ iter) {
            struct AstNode *expr = alt_parse_expression_from_string(test->expr);
            if (expr == NULL) {
                if (test->parse_ok) {
                    fprintf(stderr, "*** Error parsing expression: %s\n", test->expr);
                    return 1;
                }
            } else {
                ast_free(expr);
                if (!test->parse_ok) {
                    fprintf(stderr, "*** Expected error parsing expression, but was ok: %s\n", test->expr);
                    return 1;
                }
            }
        }
    }
    res_end = clock_gettime(CLOCK_MONOTONIC, &ts_end);
    assert(res_start == 0); (void)res_start;
    assert(res_end == 0); (void)res_end;

    tv_start = TS_TO_TV(ts_start);
    tv_end = TS_TO_TV(ts_end);

    struct timeval tv_alt_bench;
    timersub(&tv_end, &tv_start, &tv_alt_bench);

    // TODO: devide by ITERS and number of tests
    printf("Benchmark result:\n");
    printf("recursive descent: %zd.%06zu\n", (ssize_t)tv_bench.tv_sec, (size_t)tv_bench.tv_usec);
    printf("pratt:             %zd.%06zu\n", (ssize_t)tv_alt_bench.tv_sec, (size_t)tv_alt_bench.tv_usec);

    return status;
}
#else

// ========================================================================== //
//                                                                            //
//                                   Main                                     //
//                                                                            //
// ========================================================================== //

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <EXPRESSION>...\n", argc > 0 ? argv[0] : "minmath");
        return 1;
    }
    int status = 0;
    for (int argind = 1; argind < argc; ++ argind) {
        struct AstNode *expr;

        expr = parse_expression_from_string(argv[argind]);
        if (expr != NULL) {
            int value = ast_execute(expr);
            printf("%s = %d\n", argv[argind], value);
            ast_free(expr);
        } else {
            fprintf(stderr, "%s = ERROR: %s\n", argv[argind], strerror(errno));
            status = 1;
        }

        expr = alt_parse_expression_from_string(argv[argind]);
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
#endif
