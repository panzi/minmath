#include "tokenizer.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

bool token_is_error(enum TokenType token) {
    switch (token) {
    case TOK_ERROR_TOKEN:
    case TOK_ERROR_MEMORY:
        return true;
    
    default:
        return false;
    }
}

const char *get_token_name(enum TokenType token) {
    switch (token) {
    case TOK_START:        return "<START>";
    case TOK_EOF:          return "<EOF>";
    case TOK_ERROR_TOKEN:  return "<ILLEGAL TOKEN>";
    case TOK_ERROR_MEMORY: return "<MEMORY ERROR>";
    case TOK_PLUS:         return "+";
    case TOK_MINUS:        return "-";
    case TOK_MUL:          return "*";
    case TOK_DIV:          return "/";
    case TOK_MOD:          return "%";
    case TOK_INT:          return "<INTEGER>";
    case TOK_IDENT:        return "<IDENTIFIER>";
    case TOK_LPAREN:       return "(";
    case TOK_RPAREN:       return ")";
    case TOK_QUEST:        return "?";
    case TOK_COLON:        return ":";
    case TOK_BIT_OR:       return "|";
    case TOK_BIT_XOR:      return "^";
    case TOK_BIT_AND:      return "&";
    case TOK_BIT_NEG:      return "~";
    case TOK_NOT:          return "!";
    case TOK_AND:          return "&&";
    case TOK_OR:           return "||";
    case TOK_LT:           return "<";
    case TOK_GT:           return ">";
    case TOK_LE:           return "<=";
    case TOK_GE:           return ">=";
    case TOK_EQ:           return "==";
    case TOK_NE:           return "!=";
    default:
        assert(false);
        return "illegal token enum value";
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
            tokenizer->token_pos = tokenizer->input_pos;
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

    tokenizer->token_pos = tokenizer->input_pos;

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
        case '%':
        case '(':
        case ')':
        case '?':
        case ':':
        case '~':
        case '^':
            tokenizer->input_pos ++;
            return tokenizer->token = (enum TokenType) ch;

        case '&':
            tokenizer->input_pos ++;
            if (tokenizer->input[tokenizer->input_pos] == '&') {
                tokenizer->input_pos ++;
                return tokenizer->token = TOK_AND;
            }
            return tokenizer->token = TOK_BIT_AND;

        case '|':
            tokenizer->input_pos ++;
            if (tokenizer->input[tokenizer->input_pos] == '|') {
                tokenizer->input_pos ++;
                return tokenizer->token = TOK_OR;
            }
            return tokenizer->token = TOK_BIT_OR;

        case '<':
            tokenizer->input_pos ++;
            if (tokenizer->input[tokenizer->input_pos] == '=') {
                tokenizer->input_pos ++;
                return tokenizer->token = TOK_LE;
            }
            return tokenizer->token = TOK_LT;

        case '>':
            tokenizer->input_pos ++;
            if (tokenizer->input[tokenizer->input_pos] == '=') {
                tokenizer->input_pos ++;
                return tokenizer->token = TOK_GE;
            }
            return tokenizer->token = TOK_GT;

        case '=':
            if (tokenizer->input[tokenizer->input_pos + 1] != '=') {
                return tokenizer->token = TOK_ERROR_TOKEN;
            }
            tokenizer->input_pos += 2;
            return tokenizer->token = TOK_EQ;

        case '!':
            tokenizer->input_pos ++;
            if (tokenizer->input[tokenizer->input_pos] == '=') {
                tokenizer->input_pos ++;
                return tokenizer->token = TOK_NE;
            }
            return tokenizer->token = TOK_NOT;

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
                    return tokenizer->token = TOK_ERROR_MEMORY;
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
                return tokenizer->token = TOK_ERROR_TOKEN;
            }
    }
}
