#include "tokenizer.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

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
