#include "ast.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>

bool ast_is_binary(const struct AstNode *expr) {
    switch (expr->type) {
        case NODE_ADD:
        case NODE_SUB:
        case NODE_MUL:
        case NODE_DIV:
        case NODE_MOD:
        case NODE_AND:
        case NODE_OR:
        case NODE_LT:
        case NODE_GT:
        case NODE_LE:
        case NODE_GE:
        case NODE_EQ:
        case NODE_NE:
        case NODE_BIT_AND:
        case NODE_BIT_OR:
        case NODE_BIT_XOR:
        case NODE_LSHIFT:
        case NODE_RSHIFT:
            return true;

        default:
            return false;
    }
}

bool ast_is_unary(const struct AstNode *expr) {
    switch (expr->type) {
        case NODE_NEG:
        case NODE_BIT_NEG:
        case NODE_NOT:
            return true;

        default:
            return false;
    }
}

#define AST_BUFFER_MAX_CAPACITY (((size_t)2 * 1024 * 1024 * 1024) / sizeof(struct AstNode))

struct AstNode *ast_alloc(struct AstBuffer *buffer) {
    // malloc()ating a list of chunks, not realloc()ating a buffer, because we
    // use pointers all over the place, which would be invalidated on realloc().
    if (buffer->last == NULL || buffer->last->size == buffer->last->capacity) {
        size_t capacity = buffer->last == NULL ? 0 : buffer->last->capacity;
        if (capacity == 0) {
            capacity = 16;
        } else if (capacity >= AST_BUFFER_MAX_CAPACITY / 2) {
            capacity = AST_BUFFER_MAX_CAPACITY;
        } else {
            capacity *= 2;
        }
        struct AstBufferChunk *new_chunk = malloc(sizeof(struct AstBufferChunk) + capacity * sizeof(struct AstNode));
        if (new_chunk == NULL) {
            return NULL;
        }

        new_chunk->size     = 0;
        new_chunk->capacity = capacity;
        new_chunk->next     = NULL;

        if (buffer->first == NULL) {
            buffer->first = new_chunk;
        } else {
            buffer->last->next = new_chunk;
        }
        buffer->last = new_chunk;
    }

    struct AstNode *node = &buffer->last->buffer[buffer->last->size];
    ++ buffer->last->size;
    return node;
}

void ast_buffer_clear(struct AstBuffer *buffer) {
    struct AstBufferChunk *last = buffer->last;
    for (struct AstBufferChunk *ptr = buffer->first; ptr != last;) {
        struct AstBufferChunk *chunk = ptr;
        for (size_t index = 0; index < chunk->size; ++ index) {
            struct AstNode *node = &chunk->buffer[index];
            if (node->type == NODE_VAR) {
                free(node->data.ident);
            }
        }
        ptr = ptr->next;
        free(chunk);
    }

    if (last != NULL) {
        for (size_t index = 0; index < last->size; ++ index) {
            struct AstNode *node = &last->buffer[index];
            if (node->type == NODE_VAR) {
                free(node->data.ident);
            }
        }
        last->size = 0;
        buffer->first = last;
    }
}

void ast_buffer_free(struct AstBuffer *buffer) {
    for (struct AstBufferChunk *ptr = buffer->first; ptr != NULL;) {
        struct AstBufferChunk *chunk = ptr;
        for (size_t index = 0; index < chunk->size; ++ index) {
            struct AstNode *node = &chunk->buffer[index];
            if (node->type == NODE_VAR) {
                free(node->data.ident);
            }
        }
        ptr = ptr->next;
        free(chunk);
    }

    buffer->first = NULL;
    buffer->last  = NULL;
}

struct AstNode *ast_create_terneary(struct AstBuffer *buffer, struct AstNode *cond, struct AstNode *then_expr, struct AstNode *else_expr) {
    struct AstNode *node = ast_alloc(buffer);
    if (node == NULL) {
        return NULL;
    }

    *node = (struct AstNode){
        .type = NODE_IF,
        .data = {
            .terneary = {
                .cond      = cond,
                .then_expr = then_expr,
                .else_expr = else_expr,
            }
        }
    };

    return node;
}

struct AstNode *ast_create_binary(struct AstBuffer *buffer, enum NodeType type, struct AstNode *lhs, struct AstNode *rhs) {
    assert(
        type == NODE_ADD ||
        type == NODE_SUB ||
        type == NODE_MUL ||
        type == NODE_DIV ||
        type == NODE_MOD ||
        type == NODE_OR ||
        type == NODE_AND ||
        type == NODE_LT ||
        type == NODE_GT ||
        type == NODE_LE ||
        type == NODE_GE ||
        type == NODE_EQ ||
        type == NODE_NE ||
        type == NODE_BIT_OR ||
        type == NODE_BIT_XOR ||
        type == NODE_BIT_AND ||
        type == NODE_LSHIFT ||
        type == NODE_RSHIFT
    );

    struct AstNode *node = ast_alloc(buffer);
    if (node == NULL) {
        return NULL;
    }

    *node = (struct AstNode){
        .type = type,
        .data = {
            .binary = {
                .lhs = lhs,
                .rhs = rhs,
            }
        }
    };

    return node;
}

struct AstNode *ast_create_unary(struct AstBuffer *buffer, enum NodeType type, struct AstNode *child) {
    assert(type == NODE_NEG || type == NODE_BIT_NEG || type == NODE_NOT);

    struct AstNode *node = ast_alloc(buffer);
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

struct AstNode *ast_create_int(struct AstBuffer *buffer, int value) {
    struct AstNode *node = ast_alloc(buffer);
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

struct AstNode *ast_create_var(struct AstBuffer *buffer, char *name) {
    struct AstNode *node = ast_alloc(buffer);
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

void ast_print(FILE *stream, const struct AstNode *expr) {
    if (ast_is_binary(expr)) {
        fputc('(', stream);
        ast_print(stream, expr->data.binary.lhs);
        switch (expr->type) {
            case NODE_ADD:
                fprintf(stream, " + ");
                break;

            case NODE_SUB:
                fprintf(stream, " - ");
                break;

            case NODE_MUL:
                fprintf(stream, " * ");
                break;

            case NODE_DIV:
                fprintf(stream, " / ");
                break;

            case NODE_MOD:
                fprintf(stream, " %% ");
                break;

            case NODE_AND:
                fprintf(stream, " && ");
                break;

            case NODE_OR:
                fprintf(stream, " || ");
                break;

            case NODE_LT:
                fprintf(stream, " < ");
                break;

            case NODE_GT:
                fprintf(stream, " > ");
                break;

            case NODE_LE:
                fprintf(stream, " <= ");
                break;

            case NODE_GE:
                fprintf(stream, " >= ");
                break;

            case NODE_EQ:
                fprintf(stream, " == ");
                break;

            case NODE_NE:
                fprintf(stream, " != ");
                break;

            case NODE_BIT_AND:
                fprintf(stream, " & ");
                break;

            case NODE_BIT_OR:
                fprintf(stream, " | ");
                break;

            case NODE_BIT_XOR:
                fprintf(stream, " ^ ");
                break;

            case NODE_LSHIFT:
                fprintf(stream, " << ");
                break;

            case NODE_RSHIFT:
                fprintf(stream, " >> ");
                break;

            default:
                assert(false);
                fprintf(stream, " <?> ");
                break;
        }

        ast_print(stream, expr->data.binary.rhs);
        fputc(')', stream);
    } else if (expr->type == NODE_IF) {
        fputc('(', stream);
        ast_print(stream, expr->data.terneary.cond);
        fprintf(stream, " ? ");
        ast_print(stream, expr->data.terneary.then_expr);
        fprintf(stream, " : ");
        ast_print(stream, expr->data.terneary.else_expr);
        fputc(')', stream);
    } else if (ast_is_unary(expr)) {
        switch (expr->type) {
            case NODE_NEG:
                fprintf(stream, "- ");
                break;

            case NODE_BIT_NEG:
                fputc('~', stream);
                break;

            case NODE_NOT:
                fputc('!', stream);
                break;

            default:
                assert(false);
                fprintf(stream, "<?> ");
                break;
        }
        ast_print(stream, expr->data.child);
    } else if (expr->type == NODE_INT) {
        fprintf(stream, "%d", expr->data.value);
    } else if (expr->type == NODE_VAR) {
        fprintf(stream, "%s", expr->data.ident);
    } else {
        assert(false);
    }
}

int param_cmp(const void *lhs, const void *rhs) {
    const struct Param *lparam = lhs;
    const struct Param *rparam = rhs;
    return strcmp(lparam->name, rparam->name);
}

void params_sort(struct Param params[], size_t param_count) {
    qsort(params, param_count, sizeof(struct Param), param_cmp);
}

int params_get(const struct Param params[], size_t param_count, const char *name) {
    size_t left  = 0;
    size_t right = param_count;

    while (left != right) {
        // left + rigth can't overflow since sizeof(struct Param) > 1
        size_t mid = (left + right) / 2;
        int cmp = strcmp(name, params[mid].name);

        if (cmp == 0) {
            return params[mid].value;
        }

        if (cmp < 0) {
            right = mid;
        } else {
            left = mid;
        }
    }

    fprintf(stderr, "*** parameter not found: %s\n", name);
    return 0;
}

// ========================================================================== //
//                                                                            //
//                             Tree Interpreter                               //
//                                                                            //
// ========================================================================== //

int ast_execute_with_environ(struct AstNode *expr) {
    assert(expr != NULL);

    switch (expr->type) {
        case NODE_ADD:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) +
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_SUB:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) -
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_MUL:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) *
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_DIV:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) /
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_MOD:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) %
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_AND:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) &&
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_OR:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) ||
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_LT:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) <
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_GT:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) >
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_LE:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) <=
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_GE:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) >=
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_EQ:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) ==
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_NE:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) !=
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_BIT_AND:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) &
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_BIT_OR:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) |
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_BIT_XOR:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) ^
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_LSHIFT:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) <<
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_RSHIFT:
            return (
                ast_execute_with_environ(expr->data.binary.lhs) >>
                ast_execute_with_environ(expr->data.binary.rhs)
            );

        case NODE_NEG:
            return -ast_execute_with_environ(expr->data.child);

        case NODE_BIT_NEG:
            return ~ast_execute_with_environ(expr->data.child);

        case NODE_NOT:
            return !ast_execute_with_environ(expr->data.child);

        case NODE_IF:
            return (
                ast_execute_with_environ(expr->data.terneary.cond) ?
                ast_execute_with_environ(expr->data.terneary.then_expr) :
                ast_execute_with_environ(expr->data.terneary.else_expr)
            );

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

int ast_execute_with_params(struct AstNode *expr, const struct Param params[], size_t param_count) {
    assert(expr != NULL);

    switch (expr->type) {
        case NODE_ADD:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) +
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_SUB:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) -
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_MUL:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) *
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_DIV:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) /
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_MOD:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) %
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_AND:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) &&
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_OR:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) ||
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_LT:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) <
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_GT:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) >
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_LE:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) <=
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_GE:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) >=
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_EQ:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) ==
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_NE:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) !=
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_BIT_AND:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) &
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_BIT_OR:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) |
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_BIT_XOR:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) ^
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_LSHIFT:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) <<
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_RSHIFT:
            return (
                ast_execute_with_params(expr->data.binary.lhs, params, param_count) >>
                ast_execute_with_params(expr->data.binary.rhs, params, param_count)
            );

        case NODE_NEG:
            return -ast_execute_with_params(expr->data.child, params, param_count);

        case NODE_BIT_NEG:
            return ~ast_execute_with_params(expr->data.child, params, param_count);

        case NODE_NOT:
            return !ast_execute_with_params(expr->data.child, params, param_count);

        case NODE_IF:
            return (
                ast_execute_with_params(expr->data.terneary.cond, params, param_count) ?
                ast_execute_with_params(expr->data.terneary.then_expr, params, param_count) :
                ast_execute_with_params(expr->data.terneary.else_expr, params, param_count)
            );

        case NODE_INT:
            return expr->data.value;

        case NODE_VAR:
            return params_get(params, param_count, expr->data.ident);

        default:
            assert(false);
            return 0;
    }
}
