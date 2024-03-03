#ifndef MINMATH_BYTECODE_H__
#define MINMATH_BYTECODE_H__
#pragma once

#include "ast.h"

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum Instr {
    INSTR_INT,
    INSTR_VAR,
    INSTR_ADD,
    INSTR_SUB,
    INSTR_MUL,
    INSTR_DIV,
    INSTR_MOD,
    INSTR_BIT_AND,
    INSTR_BIT_XOR,
    INSTR_BIT_OR,
    INSTR_LT,
    INSTR_LE,
    INSTR_GT,
    INSTR_GE,
    INSTR_EQ,
    INSTR_NE,
    INSTR_NEG,
    INSTR_BIT_NEG,
    INSTR_NOT,
    INSTR_JMP,
    INSTR_JEZ, // jump if current value equals zero, on jump replace top of stack with 0, otherwise pop stack
    INSTR_JNZ, // jump if current value doesn't equal zero, on jump replace top of stack with 1, otherwise pop stack
    INSTR_JZP, // jump if current value equals zero and always pop stack
    INSTR_BOOL,
    INSTR_LSHIFT,
    INSTR_RSHIFT,
    INSTR_RET,
};

struct Bytecode {
    uint8_t *instrs;
    size_t instrs_size;
    size_t instrs_capacity;

    char **params;
    size_t params_size;
    size_t params_capacity;

    size_t stack_size;
};

#define BYTECODE_INIT() {  \
    .instrs = NULL,        \
    .instrs_size = 0,      \
    .instrs_capacity = 0,  \
    .params = NULL,        \
    .params_size = 0,      \
    .params_capacity = 0,  \
    .stack_size = 0,       \
}

bool bytecode_compile(struct Bytecode *bytecode, const struct AstNode *expr);
bool bytecode_clone(const struct Bytecode *src, struct Bytecode *dest);
bool bytecode_optimize(struct Bytecode *bytecode);
int  bytecode_execute(const struct Bytecode *bytecode, const int *params, int *stack);
void bytecode_free(struct Bytecode *bytecode);
void bytecode_clear(struct Bytecode *bytecode);
ptrdiff_t bytecode_get_param_index(const struct Bytecode *bytecode, const char *name);
bool bytecode_set_param(const struct Bytecode *bytecode, int *params, const char *name, int value);
int *bytecode_alloc_params(const struct Bytecode *bytecode);
int *bytecode_alloc_stack(const struct Bytecode *bytecode);
void bytecode_print(const struct Bytecode *bytecode, FILE *stream);

#ifdef __cplusplus
}
#endif

#endif
