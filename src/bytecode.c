#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "bytecode.h"

#ifdef bytecode_is_ok
#undef bytecode_is_ok
#endif

union InstrArg {
    int value;
    uint32_t index;
};

#define ZERO_ARG (union InstrArg){ .value = 0 }

#define INSTR_SIZE(INSTR) (                        \
    (INSTR) == INSTR_INT ? 1 + sizeof(int) :       \
    (INSTR) == INSTR_VAR ? 1 + sizeof(uint32_t) :  \
    (INSTR) == INSTR_JEZ ||                        \
    (INSTR) == INSTR_JNZ ||                        \
    (INSTR) == INSTR_JMP ||                        \
    (INSTR) == INSTR_JNP ? 1 + sizeof(uint32_t) :  \
    1                                              \
)

static bool bytecode_add_instr(struct Bytecode *bytecode, enum Instr instr, union InstrArg arg) {
    size_t instr_size = INSTR_SIZE(instr);

    if (bytecode->instrs_capacity - bytecode->instrs_size < instr_size) {
        uint32_t new_capacity;
        if (bytecode->instrs_capacity == 0) {
            new_capacity = sizeof(union InstrArg) * 2;
        } else if (bytecode->instrs_capacity > INT32_MAX / 2) {
            errno = ENOMEM;
            return false;
        } else {
            new_capacity = bytecode->instrs_capacity * 2;
        }
        assert(new_capacity - bytecode->instrs_size >= instr_size);
        uint8_t *instrs = realloc(bytecode->instrs, new_capacity);
        if (instrs == NULL) {
            return false;
        }
        bytecode->instrs = instrs;
        bytecode->instrs_capacity = new_capacity;
    }

    bytecode->instrs[bytecode->instrs_size] = instr;

    if (instr == INSTR_INT) {
        memcpy(bytecode->instrs + bytecode->instrs_size + 1, &arg.value, sizeof(arg.value));
    } else if (instr == INSTR_VAR) {
        memcpy(bytecode->instrs + bytecode->instrs_size + 1, &arg.index, sizeof(arg.index));
    } else if (instr == INSTR_JEZ || instr == INSTR_JNZ || instr == INSTR_JMP || instr == INSTR_JNP) {
        memcpy(bytecode->instrs + bytecode->instrs_size + 1, &arg.index, sizeof(arg.index));
    }

    bytecode->instrs_size += instr_size;
    return true;
}

static int32_t bytecode_add_param(struct Bytecode *bytecode, const char *name) {
    int32_t index = bytecode_get_param_index(bytecode, name);
    if (index >= 0) {
        return index;
    }

    if (bytecode->params_size == bytecode->params_capacity) {
        uint32_t new_capacity;
        if (bytecode->params_capacity == 0) {
            new_capacity = 4;
        } else if (bytecode->params_capacity > INT32_MAX / 2) {
            errno = ENOMEM;
            return -1;
        } else {
            new_capacity = bytecode->params_capacity * 2;
        }
        char **params = realloc(bytecode->params, new_capacity);
        if (params == NULL) {
            return -1;
        }
        bytecode->params = params;
        bytecode->params_capacity = new_capacity;
    }

    index = bytecode->params_size;
    char *new_name = strdup(name);
    if (new_name == NULL) {
        return -1;
    }
    bytecode->params[index] = new_name;
    ++ bytecode->params_size;

    return index;
}

static bool bytecode_compile_ast(struct Bytecode *bytecode, const struct AstNode *expr) {
    // TODO: determine stack size!
    if (expr->type == NODE_AND) {
        if (!bytecode_compile_ast(bytecode, expr->data.binary.lhs)) {
            return false;
        }

        uint32_t jmp_arg_index = bytecode->instrs_size + 1;

        if (!bytecode_add_instr(bytecode, INSTR_JEZ, ZERO_ARG)) {
            return false;
        }

        if (!bytecode_compile_ast(bytecode, expr->data.binary.rhs)) {
            return false;
        }

        uint32_t jmp_target = bytecode->instrs_size;
        memcpy(bytecode->instrs + jmp_arg_index, &jmp_target, sizeof(jmp_target));

        return true;
    } else if (expr->type == NODE_OR) {
        if (!bytecode_compile_ast(bytecode, expr->data.binary.lhs)) {
            return false;
        }

        uint32_t jmp_arg_index = bytecode->instrs_size + 1;

        if (!bytecode_add_instr(bytecode, INSTR_JNZ, ZERO_ARG)) {
            return false;
        }

        if (!bytecode_compile_ast(bytecode, expr->data.binary.rhs)) {
            return false;
        }

        uint32_t jmp_target = bytecode->instrs_size;
        memcpy(bytecode->instrs + jmp_arg_index, &jmp_target, sizeof(jmp_target));

        return true;
    } else if (ast_is_binary(expr)) {
        if (!bytecode_compile_ast(bytecode, expr->data.binary.lhs)) {
            return false;
        }

        if (!bytecode_compile_ast(bytecode, expr->data.binary.rhs)) {
            return false;
        }

        switch (expr->type) {
            case NODE_ADD:
                return bytecode_add_instr(bytecode, INSTR_ADD, ZERO_ARG);

            case NODE_SUB:
                return bytecode_add_instr(bytecode, INSTR_SUB, ZERO_ARG);

            case NODE_MUL:
                return bytecode_add_instr(bytecode, INSTR_MUL, ZERO_ARG);

            case NODE_DIV:
                return bytecode_add_instr(bytecode, INSTR_DIV, ZERO_ARG);

            case NODE_MOD:
                return bytecode_add_instr(bytecode, INSTR_MOD, ZERO_ARG);

            case NODE_LT:
                return bytecode_add_instr(bytecode, INSTR_LT, ZERO_ARG);

            case NODE_GT:
                return bytecode_add_instr(bytecode, INSTR_GT, ZERO_ARG);

            case NODE_LE:
                return bytecode_add_instr(bytecode, INSTR_LE, ZERO_ARG);

            case NODE_GE:
                return bytecode_add_instr(bytecode, INSTR_GE, ZERO_ARG);

            case NODE_EQ:
                return bytecode_add_instr(bytecode, INSTR_EQ, ZERO_ARG);

            case NODE_NE:
                return bytecode_add_instr(bytecode, INSTR_NE, ZERO_ARG);

            case NODE_BIT_AND:
                return bytecode_add_instr(bytecode, INSTR_BIT_AND, ZERO_ARG);

            case NODE_BIT_OR:
                return bytecode_add_instr(bytecode, INSTR_BIT_OR, ZERO_ARG);

            case NODE_BIT_XOR:
                return bytecode_add_instr(bytecode, INSTR_BIT_XOR, ZERO_ARG);

            default:
                assert(false);
                return false;
        }
    } else if (ast_is_unary(expr)) {
        if (!bytecode_compile_ast(bytecode, expr->data.child)) {
            return false;
        }

        switch (expr->type) {
            case NODE_NEG:
                return bytecode_add_instr(bytecode, INSTR_NEG, ZERO_ARG);

            case NODE_BIT_NEG:
                return bytecode_add_instr(bytecode, INSTR_BIT_NEG, ZERO_ARG);

            case NODE_NOT:
                return bytecode_add_instr(bytecode, INSTR_NOT, ZERO_ARG);

            default:
                assert(false);
                return false;
        }
    } else if (expr->type == NODE_IF) {
        if (!bytecode_compile_ast(bytecode, expr->data.terneary.cond)) {
            return false;
        }

        uint32_t cond_jmp_arg_index = bytecode->instrs_size + 1;

        if (!bytecode_add_instr(bytecode, INSTR_JNP, ZERO_ARG)) {
            return false;
        }

        if (!bytecode_compile_ast(bytecode, expr->data.terneary.then_expr)) {
            return false;
        }

        uint32_t jmp_target = bytecode->instrs_size;
        memcpy(bytecode->instrs + cond_jmp_arg_index, &jmp_target, sizeof(jmp_target));

        uint32_t then_jmp_arg_index = bytecode->instrs_size + 1;

        if (!bytecode_add_instr(bytecode, INSTR_JMP, ZERO_ARG)) {
            return false;
        }

        if (!bytecode_compile_ast(bytecode, expr->data.terneary.else_expr)) {
            return false;
        }

        jmp_target = bytecode->instrs_size;
        memcpy(bytecode->instrs + then_jmp_arg_index, &jmp_target, sizeof(jmp_target));

        return true;
    } else if (expr->type == NODE_INT) {
        return bytecode_add_instr(bytecode, INSTR_INT, (union InstrArg){ .value = expr->data.value });
    } else if (expr->type == NODE_VAR) {
        int32_t index = bytecode_add_param(bytecode, expr->data.ident);
        if (index < 0) {
            return false;
        }
        return bytecode_add_instr(bytecode, INSTR_VAR, (union InstrArg){ .index = index });
    } else {
        assert(false);
        return false;
    }
    return true;
}

static int32_t bytecode_optimize_jump_target(struct Bytecode *bytecode, uint32_t index) {
    enum Instr instr = bytecode->instrs[index];
    if (instr == INSTR_JMP) {
        uint32_t target = 0;
        memcpy(&target, bytecode->instrs + index, sizeof(target));
        if (index >= bytecode->instrs_size) {
            errno = EINVAL;
            return -1;
        }
        int32_t res = bytecode_optimize_jump_target(bytecode, target);
        if (res >= 0) {
            target = res;
            memcpy(bytecode->instrs + index, &target, sizeof(target));
        }
        return res;
    } else {
        return index;
    }
}

// Bytecode-optimizer that fixes jump targes that land on INSTR_JMP to the target
// of that instruction (transitively).
bool bytecode_optimize(struct Bytecode *bytecode) {
    for (uint32_t index = 0; index < bytecode->instrs_size;) {
        enum Instr instr = bytecode->instrs[index];
        switch (instr) {
        case INSTR_INT:
            index += 1 + sizeof(int);
            break;

        case INSTR_VAR:
            index += 1 + sizeof(uint32_t);
            break;

        case INSTR_ADD:
        case INSTR_SUB:
        case INSTR_MUL:
        case INSTR_DIV:
        case INSTR_MOD:
        case INSTR_BIT_AND:
        case INSTR_BIT_XOR:
        case INSTR_BIT_OR:
        case INSTR_LT:
        case INSTR_LE:
        case INSTR_GT:
        case INSTR_GE:
        case INSTR_EQ:
        case INSTR_NE:
        case INSTR_NEG:
        case INSTR_BIT_NEG:
        case INSTR_NOT:
        case INSTR_RET:
            index += 1;
            break;

        case INSTR_JMP:
        case INSTR_JEZ:
        case INSTR_JNZ:
        case INSTR_JNP:
        {
            index += 1;
            if (index >= bytecode->instrs_size) {
                errno = EINVAL;
                return false;
            }
            uint32_t target;
            memcpy(&target, bytecode->instrs + index, sizeof(target));

            int32_t res = bytecode_optimize_jump_target(bytecode, target);

            if (res < 0) {
                return false;
            }

            target = res;
            memcpy(bytecode->instrs + index, &target, sizeof(target));

            index += sizeof(uint32_t);
            break;
        }
        default:
            assert(false);
            errno = EINVAL;
            return false;
        }
    }
    return true;
}

struct Bytecode bytecode_compile(const struct AstNode *expr) {
    struct Bytecode bytecode = BYTECODE_INIT();

    if (!bytecode_compile_ast(&bytecode, expr)) {
        bytecode_free(&bytecode);
        return bytecode;
    }

    if (!bytecode_add_instr(&bytecode, INSTR_RET, ZERO_ARG)) {
        bytecode_free(&bytecode);
        return bytecode;
    }

    return bytecode;
}

bool bytecode_is_ok(const struct Bytecode *bytecode) {
    return BYTECODE_IS_OK(bytecode);
}

int bytecode_execute(const struct Bytecode *bytecode, const int *params) {
    // TODO
    assert(false);
    return -1;
}

void bytecode_free(struct Bytecode *bytecode) {
    free(bytecode->instrs);
    for (uint32_t index = 0; index < bytecode->params_size; ++ index) {
        free(bytecode->params[index]);
    }
    free(bytecode->params);

    bytecode->instrs = NULL;
    bytecode->instrs_size = 0;
    bytecode->instrs_capacity = 0;

    bytecode->params = NULL;
    bytecode->params_size = 0;
    bytecode->params_capacity = 0;

    bytecode->stack_size = 0;
}

int *bytecode_alloc_params(const struct Bytecode *bytecode) {
    return calloc(bytecode->params_size, sizeof(int));
}

int32_t bytecode_get_param_index(const struct Bytecode *bytecode, const char *name) {
    for (int32_t index = 0; index < bytecode->params_size; ++ index) {
        if (strcmp(bytecode->params[index], name) == 0) {
            return index;
        }
    }
    return -1;
}

bool bytecode_set_param(const struct Bytecode *bytecode, int *params, const char *name, int value) {
    int32_t index = bytecode_get_param_index(bytecode, name);
    if (index < 0) {
        return false;
    }
    params[index] = value;
    return true;
}
