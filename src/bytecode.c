#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <inttypes.h>

#include "bytecode.h"

#ifdef bytecode_is_ok
#undef bytecode_is_ok
#endif

union InstrArg {
    int value;
    size_t index;
};

#define ZERO_ARG (union InstrArg){ .value = 0 }

#define INSTR_SIZE(INSTR) (                      \
    (INSTR) == INSTR_INT ? 1 + sizeof(int) :     \
    (INSTR) == INSTR_VAR ? 1 + sizeof(size_t) :  \
    (INSTR) == INSTR_JEZ ||                      \
    (INSTR) == INSTR_JNZ ||                      \
    (INSTR) == INSTR_JMP ||                      \
    (INSTR) == INSTR_JZP ? 1 + sizeof(size_t) :  \
    1                                            \
)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

static bool bytecode_add_instr(struct Bytecode *bytecode, enum Instr instr, union InstrArg arg) {
    size_t instr_size = INSTR_SIZE(instr);

    if (bytecode->instrs_capacity - bytecode->instrs_size < instr_size) {
        size_t new_capacity;
        if (bytecode->instrs_capacity == 0) {
            new_capacity = sizeof(union InstrArg) * 2;
        } else if (bytecode->instrs_capacity > PTRDIFF_MAX / 2) {
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

#ifndef NDEBUG
        // make it clear if some of the bytes aren't initialized correctly
        memset(
            instrs + bytecode->instrs_size,
            0xFF,
            new_capacity - bytecode->instrs_size
        );
#endif

        bytecode->instrs = instrs;
        bytecode->instrs_capacity = new_capacity;
    }

    bytecode->instrs[bytecode->instrs_size] = instr;

    if (instr == INSTR_INT) {
        memcpy(bytecode->instrs + bytecode->instrs_size + 1, &arg.value, sizeof(arg.value));
    } else if (instr == INSTR_VAR) {
        memcpy(bytecode->instrs + bytecode->instrs_size + 1, &arg.index, sizeof(arg.index));
    } else if (instr == INSTR_JEZ || instr == INSTR_JNZ || instr == INSTR_JMP || instr == INSTR_JZP) {
        memcpy(bytecode->instrs + bytecode->instrs_size + 1, &arg.index, sizeof(arg.index));
    }

    bytecode->instrs_size += instr_size;
    return true;
}

static ptrdiff_t bytecode_add_param(struct Bytecode *bytecode, const char *name) {
    ptrdiff_t index = bytecode_get_param_index(bytecode, name);
    if (index >= 0) {
        return index;
    }

    if (bytecode->params_size == bytecode->params_capacity) {
        size_t new_capacity;
        if (bytecode->params_capacity == 0) {
            new_capacity = 4;
        } else if (bytecode->params_capacity > PTRDIFF_MAX / 2 / sizeof(char*)) {
            errno = ENOMEM;
            return -1;
        } else {
            new_capacity = bytecode->params_capacity * 2;
        }
        char **params = realloc(bytecode->params, new_capacity * sizeof(char*));
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

static ptrdiff_t bytecode_compile_ast(struct Bytecode *bytecode, const struct AstNode *expr) {
    if (expr->type == NODE_AND) {
        ptrdiff_t lhs_stack = bytecode_compile_ast(bytecode, expr->data.binary.lhs);
        if (lhs_stack < 0) {
            return lhs_stack;
        }

        size_t jmp_arg_index = bytecode->instrs_size + 1;

        if (!bytecode_add_instr(bytecode, INSTR_JEZ, ZERO_ARG)) {
            return -1;
        }

        if (!bytecode_add_instr(bytecode, INSTR_POP, ZERO_ARG)) {
            return -1;
        }

        ptrdiff_t rhs_stack = bytecode_compile_ast(bytecode, expr->data.binary.rhs);
        if (rhs_stack < 0) {
            return rhs_stack;
        }

        size_t jmp_target = bytecode->instrs_size;
        memcpy(bytecode->instrs + jmp_arg_index, &jmp_target, sizeof(jmp_target));

        return MAX(lhs_stack, rhs_stack);
    } else if (expr->type == NODE_OR) {
        ptrdiff_t lhs_stack = bytecode_compile_ast(bytecode, expr->data.binary.lhs);
        if (lhs_stack < 0) {
            return lhs_stack;
        }

        size_t jmp_arg_index = bytecode->instrs_size + 1;

        if (!bytecode_add_instr(bytecode, INSTR_JNZ, ZERO_ARG)) {
            return -1;
        }

        if (!bytecode_add_instr(bytecode, INSTR_POP, ZERO_ARG)) {
            return -1;
        }

        ptrdiff_t rhs_stack = bytecode_compile_ast(bytecode, expr->data.binary.rhs);
        if (rhs_stack < 0) {
            return rhs_stack;
        }

        size_t jmp_target = bytecode->instrs_size;
        memcpy(bytecode->instrs + jmp_arg_index, &jmp_target, sizeof(jmp_target));

        return MAX(lhs_stack, rhs_stack);
    } else if (ast_is_binary(expr)) {
        ptrdiff_t lhs_stack = bytecode_compile_ast(bytecode, expr->data.binary.lhs);
        if (lhs_stack < 0) {
            return lhs_stack;
        }

        ptrdiff_t rhs_stack = bytecode_compile_ast(bytecode, expr->data.binary.rhs);
        if (rhs_stack < 0) {
            return rhs_stack;
        }

        switch (expr->type) {
            case NODE_ADD:
                if (!bytecode_add_instr(bytecode, INSTR_ADD, ZERO_ARG)) {
                    return -1;
                }
                break;

            case NODE_SUB:
                if (!bytecode_add_instr(bytecode, INSTR_SUB, ZERO_ARG)) {
                    return -1;
                }
                break;

            case NODE_MUL:
                if (!bytecode_add_instr(bytecode, INSTR_MUL, ZERO_ARG)) {
                    return -1;
                }
                break;

            case NODE_DIV:
                if (!bytecode_add_instr(bytecode, INSTR_DIV, ZERO_ARG)) {
                    return -1;
                }
                break;

            case NODE_MOD:
                if (!bytecode_add_instr(bytecode, INSTR_MOD, ZERO_ARG)) {
                    return -1;
                }
                break;

            case NODE_LT:
                if (!bytecode_add_instr(bytecode, INSTR_LT, ZERO_ARG)) {
                    return -1;
                }
                break;

            case NODE_GT:
                if (!bytecode_add_instr(bytecode, INSTR_GT, ZERO_ARG)) {
                    return -1;
                }
                break;

            case NODE_LE:
                if (!bytecode_add_instr(bytecode, INSTR_LE, ZERO_ARG)) {
                    return -1;
                }
                break;

            case NODE_GE:
                if (!bytecode_add_instr(bytecode, INSTR_GE, ZERO_ARG)) {
                    return -1;
                }
                break;

            case NODE_EQ:
                if (!bytecode_add_instr(bytecode, INSTR_EQ, ZERO_ARG)) {
                    return -1;
                }
                break;

            case NODE_NE:
                if (!bytecode_add_instr(bytecode, INSTR_NE, ZERO_ARG)) {
                    return -1;
                }
                break;

            case NODE_BIT_AND:
                if (!bytecode_add_instr(bytecode, INSTR_BIT_AND, ZERO_ARG)) {
                    return -1;
                }
                break;

            case NODE_BIT_OR:
                if (!bytecode_add_instr(bytecode, INSTR_BIT_OR, ZERO_ARG)) {
                    return -1;
                }
                break;

            case NODE_BIT_XOR:
                if (!bytecode_add_instr(bytecode, INSTR_BIT_XOR, ZERO_ARG)) {
                    return -1;
                }
                break;

            default:
                assert(false);
                errno = EINVAL;
                return -1;
        }

        if (rhs_stack >= PTRDIFF_MAX) {
            errno = ENOMEM;
            return -1;
        }
        // the lhs result needs to stay on the stack while the rhs is calculated
        ++ rhs_stack;

        return MAX(lhs_stack, rhs_stack);
    } else if (ast_is_unary(expr)) {
        ptrdiff_t stack_size = bytecode_compile_ast(bytecode, expr->data.child);
        if (stack_size < 0) {
            return stack_size;
        }

        switch (expr->type) {
            case NODE_NEG:
                if (!bytecode_add_instr(bytecode, INSTR_NEG, ZERO_ARG)) {
                    return -1;
                }
                break;

            case NODE_BIT_NEG:
                if (!bytecode_add_instr(bytecode, INSTR_BIT_NEG, ZERO_ARG)) {
                    return -1;
                }
                break;

            case NODE_NOT:
                if (!bytecode_add_instr(bytecode, INSTR_NOT, ZERO_ARG)) {
                    return -1;
                }
                break;

            default:
                assert(false);
                errno = EINVAL;
                return -1;
        }

        return stack_size;
    } else if (expr->type == NODE_IF) {
        ptrdiff_t cond_stack = bytecode_compile_ast(bytecode, expr->data.terneary.cond);
        if (cond_stack < 0) {
            return cond_stack;
        }

        size_t cond_jmp_arg_index = bytecode->instrs_size + 1;

        if (!bytecode_add_instr(bytecode, INSTR_JZP, ZERO_ARG)) {
            return -1;
        }

        ptrdiff_t then_stack = bytecode_compile_ast(bytecode, expr->data.terneary.then_expr);
        if (then_stack < 0) {
            return then_stack;
        }

        size_t then_jmp_arg_index = bytecode->instrs_size + 1;

        if (!bytecode_add_instr(bytecode, INSTR_JMP, ZERO_ARG)) {
            return -1;
        }

        size_t jmp_target = bytecode->instrs_size;
        memcpy(bytecode->instrs + cond_jmp_arg_index, &jmp_target, sizeof(jmp_target));

        int32_t else_stack = bytecode_compile_ast(bytecode, expr->data.terneary.else_expr);
        if (else_stack < 0) {
            return else_stack;
        }

        jmp_target = bytecode->instrs_size;
        memcpy(bytecode->instrs + then_jmp_arg_index, &jmp_target, sizeof(jmp_target));

        ptrdiff_t stack_size = MAX(cond_stack, then_stack);
        return MAX(stack_size, else_stack);
    } else if (expr->type == NODE_INT) {
        if (!bytecode_add_instr(bytecode, INSTR_INT, (union InstrArg){ .value = expr->data.value })) {
            return -1;
        }
        return 1;
    } else if (expr->type == NODE_VAR) {
        ptrdiff_t index = bytecode_add_param(bytecode, expr->data.ident);
        if (index < 0) {
            return -1;
        }
        if (!bytecode_add_instr(bytecode, INSTR_VAR, (union InstrArg){ .index = index })) {
            return -1;
        }
        return 1;
    } else {
        fprintf(stderr, "*** illegal node type: %d\n", expr->type);
        assert(false);
        errno = EINVAL;
        return -1;
    }
}

static ptrdiff_t bytecode_optimize_jump_target(struct Bytecode *bytecode, size_t index) {
    enum Instr instr = bytecode->instrs[index];
    if (instr == INSTR_JMP) {
        size_t target = 0;
        memcpy(&target, bytecode->instrs + index + 1, sizeof(target));
        if (index >= bytecode->instrs_size) {
            errno = EINVAL;
            return -1;
        }
        ptrdiff_t res = bytecode_optimize_jump_target(bytecode, target);
        if (res >= 0) {
            target = res;
            if (bytecode->instrs[target] == INSTR_RET) {
                memset(bytecode->instrs + index, INSTR_RET, 1 + sizeof(target));
            } else {
                memcpy(bytecode->instrs + index + 1, &target, sizeof(target));
            }
        }
        return res;
    } else {
        return index;
    }
}

// Bytecode-optimizer that fixes jump targes that land on INSTR_JMP to the target
// of that instruction (transitively).
bool bytecode_optimize(struct Bytecode *bytecode) {
    for (size_t index = 0; index < bytecode->instrs_size;) {
        enum Instr instr = bytecode->instrs[index];
        switch (instr) {
        case INSTR_INT:
            index += 1 + sizeof(int);
            break;

        case INSTR_VAR:
            index += 1 + sizeof(size_t);
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
        case INSTR_POP:
        case INSTR_RET:
            ++ index;
            break;

        case INSTR_JMP:
        case INSTR_JEZ:
        case INSTR_JNZ:
        case INSTR_JZP:
        {
            ++ index;
            if (index >= bytecode->instrs_size) {
                errno = EINVAL;
                return false;
            }
            size_t target;
            memcpy(&target, bytecode->instrs + index, sizeof(target));

            ptrdiff_t res = bytecode_optimize_jump_target(bytecode, target);

            if (res < 0) {
                return false;
            }

            target = res;
            if (bytecode->instrs[target] == INSTR_RET) {
                if (instr == INSTR_JZP) {
                    bytecode->instrs[index - 1] = INSTR_POP;
                    memset(bytecode->instrs + index, INSTR_RET, sizeof(target));
                } else {
                    memset(bytecode->instrs + index - 1, INSTR_RET, 1 + sizeof(target));
                }
            } else {
                memcpy(bytecode->instrs + index, &target, sizeof(target));
            }

            index += sizeof(target);
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

bool bytecode_compile(struct Bytecode *bytecode, const struct AstNode *expr) {
    ptrdiff_t stack_size = bytecode_compile_ast(bytecode, expr);

    if (stack_size < 0) {
        bytecode_clear(bytecode);
        return false;
    }

    bytecode->stack_size = stack_size;

    if (!bytecode_add_instr(bytecode, INSTR_RET, ZERO_ARG)) {
        bytecode_clear(bytecode);
        return false;
    }

    return true;
}

bool bytecode_is_ok(const struct Bytecode *bytecode) {
    return BYTECODE_IS_OK(bytecode);
}

#if (defined(__GNUC__) || defined(__clang__)) && !defined(MINMATH_ADDRESS_FROM_LABEL)
#   define MINMATH_ADDRESS_FROM_LABEL
#endif

#ifdef MINMATH_ADDRESS_FROM_LABEL
#   define DISPATCH_INSTR \
        assert(instr_ptr < bytecode->instrs_size); \
        goto *(jmptbl[instrs[instr_ptr]]);
#   define BEGIN_EXEC DISPATCH_INSTR
#   define JMP_LABEL(NAME) DO_ ## NAME:
#   define NEXT_INSTR DISPATCH_INSTR
#   define END_EXEC
#else
#   define BEGIN_EXEC \
        const size_t instrs_size = bytecode->instrs_size; \
        while (instr_ptr < instrs_size) { \
            switch (instrs[instr_ptr]) {
#   define JMP_LABEL(NAME) case INSTR_ ## NAME:
#   define NEXT_INSTR break;
#   define END_EXEC } }
#endif

int bytecode_execute(const struct Bytecode *bytecode, const int *params, int *stack) {
#ifdef MINMATH_ADDRESS_FROM_LABEL
    static const void *jmptbl[] = {
        [INSTR_INT]     = &&DO_INT,
        [INSTR_VAR]     = &&DO_VAR,
        [INSTR_ADD]     = &&DO_ADD,
        [INSTR_SUB]     = &&DO_SUB,
        [INSTR_MUL]     = &&DO_MUL,
        [INSTR_DIV]     = &&DO_DIV,
        [INSTR_MOD]     = &&DO_MOD,
        [INSTR_BIT_AND] = &&DO_BIT_AND,
        [INSTR_BIT_XOR] = &&DO_BIT_XOR,
        [INSTR_BIT_OR]  = &&DO_BIT_OR,
        [INSTR_LT]      = &&DO_LT,
        [INSTR_LE]      = &&DO_LE,
        [INSTR_GT]      = &&DO_GT,
        [INSTR_GE]      = &&DO_GE,
        [INSTR_EQ]      = &&DO_EQ,
        [INSTR_NE]      = &&DO_NE,
        [INSTR_NEG]     = &&DO_NEG,
        [INSTR_BIT_NEG] = &&DO_BIT_NEG,
        [INSTR_NOT]     = &&DO_NOT,
        [INSTR_JMP]     = &&DO_JMP,
        [INSTR_JEZ]     = &&DO_JEZ,
        [INSTR_JNZ]     = &&DO_JNZ,
        [INSTR_JZP]     = &&DO_JZP,
        [INSTR_POP]     = &&DO_POP,
        [INSTR_RET]     = &&DO_RET,
    };
#endif

    const uint8_t *instrs = bytecode->instrs;
    size_t instr_ptr = 0;
    size_t stack_ptr = 0;
    size_t addr;

    BEGIN_EXEC

    JMP_LABEL(INT)
    ++ instr_ptr;
    memcpy(stack + stack_ptr, instrs + instr_ptr, sizeof(int));
    ++ stack_ptr;
    instr_ptr += sizeof(int);
    NEXT_INSTR

    JMP_LABEL(VAR)
    ++ instr_ptr;
    memcpy(&addr, instrs + instr_ptr, sizeof(addr));
    instr_ptr += sizeof(addr);
    stack[stack_ptr] = params[addr];
    ++ stack_ptr;
    NEXT_INSTR

    JMP_LABEL(ADD)
    ++ instr_ptr;
    -- stack_ptr;
    stack[stack_ptr - 1] += stack[stack_ptr];
    NEXT_INSTR

    JMP_LABEL(SUB)
    ++ instr_ptr;
    -- stack_ptr;
    stack[stack_ptr - 1] -= stack[stack_ptr];
    NEXT_INSTR

    JMP_LABEL(MUL)
    ++ instr_ptr;
    -- stack_ptr;
    stack[stack_ptr - 1] *= stack[stack_ptr];
    NEXT_INSTR

    JMP_LABEL(DIV)
    ++ instr_ptr;
    -- stack_ptr;
    stack[stack_ptr - 1] /= stack[stack_ptr];
    NEXT_INSTR

    JMP_LABEL(MOD)
    ++ instr_ptr;
    -- stack_ptr;
    stack[stack_ptr - 1] %= stack[stack_ptr];
    NEXT_INSTR

    JMP_LABEL(BIT_AND)
    ++ instr_ptr;
    -- stack_ptr;
    stack[stack_ptr - 1] &= stack[stack_ptr];
    NEXT_INSTR

    JMP_LABEL(BIT_XOR)
    ++ instr_ptr;
    -- stack_ptr;
    stack[stack_ptr - 1] ^= stack[stack_ptr];
    NEXT_INSTR

    JMP_LABEL(BIT_OR)
    ++ instr_ptr;
    -- stack_ptr;
    stack[stack_ptr - 1] |= stack[stack_ptr];
    NEXT_INSTR

    JMP_LABEL(LT)
    ++ instr_ptr;
    -- stack_ptr;
    stack[stack_ptr - 1] = stack[stack_ptr - 1] < stack[stack_ptr];
    NEXT_INSTR

    JMP_LABEL(LE)
    ++ instr_ptr;
    -- stack_ptr;
    stack[stack_ptr - 1] = stack[stack_ptr - 1] <= stack[stack_ptr];
    NEXT_INSTR

    JMP_LABEL(GT)
    ++ instr_ptr;
    -- stack_ptr;
    stack[stack_ptr - 1] = stack[stack_ptr - 1] > stack[stack_ptr];
    NEXT_INSTR

    JMP_LABEL(GE)
    ++ instr_ptr;
    -- stack_ptr;
    stack[stack_ptr - 1] = stack[stack_ptr - 1] >= stack[stack_ptr];
    NEXT_INSTR

    JMP_LABEL(EQ)
    ++ instr_ptr;
    -- stack_ptr;
    stack[stack_ptr - 1] = stack[stack_ptr - 1] == stack[stack_ptr];
    NEXT_INSTR

    JMP_LABEL(NE)
    ++ instr_ptr;
    -- stack_ptr;
    stack[stack_ptr - 1] = stack[stack_ptr - 1] != stack[stack_ptr];
    NEXT_INSTR

    JMP_LABEL(NEG)
    ++ instr_ptr;
    stack[stack_ptr - 1] = -stack[stack_ptr - 1];
    NEXT_INSTR

    JMP_LABEL(BIT_NEG)
    ++ instr_ptr;
    stack[stack_ptr - 1] = ~stack[stack_ptr - 1];
    NEXT_INSTR

    JMP_LABEL(NOT)
    ++ instr_ptr;
    stack[stack_ptr - 1] = !stack[stack_ptr - 1];
    NEXT_INSTR

    JMP_LABEL(JMP)
    ++ instr_ptr;
    memcpy(&instr_ptr, instrs + instr_ptr, sizeof(instr_ptr));
    NEXT_INSTR

    JMP_LABEL(JEZ)
    if (stack[stack_ptr - 1]) {
        instr_ptr += 1 + sizeof(instr_ptr);
    } else {
        memcpy(&instr_ptr, instrs + instr_ptr + 1, sizeof(instr_ptr));
    }
    NEXT_INSTR

    JMP_LABEL(JNZ)
    if (stack[stack_ptr - 1]) {
        memcpy(&instr_ptr, instrs + instr_ptr + 1, sizeof(instr_ptr));
    } else {
        instr_ptr += 1 + sizeof(instr_ptr);
    }
    NEXT_INSTR

    JMP_LABEL(JZP)
    -- stack_ptr;
    if (stack[stack_ptr]) {
        instr_ptr += 1 + sizeof(instr_ptr);
    } else {
        memcpy(&instr_ptr, instrs + instr_ptr + 1, sizeof(instr_ptr));
    }
    NEXT_INSTR

    JMP_LABEL(POP)
    -- stack_ptr;
    ++ instr_ptr;
    NEXT_INSTR

    JMP_LABEL(RET)
    assert(stack_ptr == 1);
    -- stack_ptr;
    return stack[stack_ptr];
    NEXT_INSTR

    END_EXEC

    assert(false);
    errno = EINVAL;
    return -1;
}

void bytecode_clear(struct Bytecode *bytecode) {
    for (size_t index = 0; index < bytecode->params_size; ++ index) {
        free(bytecode->params[index]);
    }

#ifndef NDEBUG
    memset(bytecode->params, 0x00, bytecode->params_capacity * sizeof(*bytecode->params));
    memset(bytecode->instrs, 0xFF, bytecode->instrs_capacity * sizeof(*bytecode->instrs));
#endif

    bytecode->instrs_size = 0;
    bytecode->params_size = 0;
    bytecode->stack_size  = 0;
}

void bytecode_free(struct Bytecode *bytecode) {
    free(bytecode->instrs);
    for (size_t index = 0; index < bytecode->params_size; ++ index) {
        free(bytecode->params[index]);
    }
    free(bytecode->params);

    *bytecode = (struct Bytecode)BYTECODE_INIT();
}

int *bytecode_alloc_params(const struct Bytecode *bytecode) {
    return calloc(bytecode->params_size, sizeof(int));
}

int *bytecode_alloc_stack(const struct Bytecode *bytecode) {
    return calloc(bytecode->stack_size, sizeof(int));
}

ptrdiff_t bytecode_get_param_index(const struct Bytecode *bytecode, const char *name) {
    for (size_t index = 0; index < bytecode->params_size; ++ index) {
        if (strcmp(bytecode->params[index], name) == 0) {
            return index;
        }
    }
    return -1;
}

bool bytecode_set_param(const struct Bytecode *bytecode, int *params, const char *name, int value) {
    ptrdiff_t index = bytecode_get_param_index(bytecode, name);
    if (index < 0) {
        return false;
    }
    params[index] = value;
    return true;
}

void bytecode_print(const struct Bytecode *bytecode, FILE *stream) {
    int value;
    size_t addr;
    const uint8_t *instrs = bytecode->instrs;
    fprintf(stream, "stack_size: %" PRIuPTR "\n", bytecode->stack_size);

    fprintf(stream, "parameters:\n");
    for (size_t param_index = 0; param_index < bytecode->params_size; ++ param_index) {
        fprintf(stream, "%6" PRIuPTR ": %s\n", param_index, bytecode->params[param_index]);
    }

    fprintf(stream, "instructions:\n");
    for (size_t instr_ptr = 0; instr_ptr < bytecode->instrs_size;) {
        switch (instrs[instr_ptr]) {
        case INSTR_INT:
            memcpy(&value, instrs + instr_ptr + 1, sizeof(int));
            fprintf(stream, "%6" PRIuPTR ": int %d\n", instr_ptr, value);
            ++ instr_ptr;
            instr_ptr += sizeof(int);
            break;

        case INSTR_VAR:
            memcpy(&addr, instrs + instr_ptr + 1, sizeof(addr));
            fprintf(stream, "%6" PRIuPTR ": var %s\n", instr_ptr, bytecode->params[addr]);
            ++ instr_ptr;
            instr_ptr += sizeof(addr);
            break;

        case INSTR_ADD:
            fprintf(stream, "%6" PRIuPTR ": add\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_SUB:
            fprintf(stream, "%6" PRIuPTR ": sub\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_MUL:
            fprintf(stream, "%6" PRIuPTR ": mul\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_DIV:
            fprintf(stream, "%6" PRIuPTR ": div\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_MOD:
            fprintf(stream, "%6" PRIuPTR ": mod\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_BIT_AND:
            fprintf(stream, "%6" PRIuPTR ": bit_and\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_BIT_XOR:
            fprintf(stream, "%6" PRIuPTR ": bit_xor\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_BIT_OR:
            fprintf(stream, "%6" PRIuPTR ": bit_or\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_LT:
            fprintf(stream, "%6" PRIuPTR ": lt\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_LE:
            fprintf(stream, "%6" PRIuPTR ": le\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_GT:
            fprintf(stream, "%6" PRIuPTR ": gt\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_GE:
            fprintf(stream, "%6" PRIuPTR ": ge\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_EQ:
            fprintf(stream, "%6" PRIuPTR ": eq\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_NE:
            fprintf(stream, "%6" PRIuPTR ": ne\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_NEG:
            fprintf(stream, "%6" PRIuPTR ": neg\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_BIT_NEG:
            fprintf(stream, "%6" PRIuPTR ": bit_neg\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_NOT:
            fprintf(stream, "%6" PRIuPTR ": not\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_JMP:
            memcpy(&addr, instrs + instr_ptr + 1, sizeof(instr_ptr));
            fprintf(stream, "%6" PRIuPTR ": jmp %" PRIuPTR "\n", instr_ptr, addr);
            instr_ptr += 1 + sizeof(addr);
            break;

        case INSTR_JEZ:
            memcpy(&addr, instrs + instr_ptr + 1, sizeof(instr_ptr));
            fprintf(stream, "%6" PRIuPTR ": jez %" PRIuPTR "\n", instr_ptr, addr);
            instr_ptr += 1 + sizeof(addr);
            break;

        case INSTR_JNZ:
            memcpy(&addr, instrs + instr_ptr + 1, sizeof(instr_ptr));
            fprintf(stream, "%6" PRIuPTR ": jnz %" PRIuPTR "\n", instr_ptr, addr);
            instr_ptr += 1 + sizeof(addr);
            break;

        case INSTR_JZP:
            memcpy(&addr, instrs + instr_ptr + 1, sizeof(instr_ptr));
            fprintf(stream, "%6" PRIuPTR ": jzp %" PRIuPTR "\n", instr_ptr, addr);
            instr_ptr += 1 + sizeof(addr);
            break;

        case INSTR_POP:
            fprintf(stream, "%6" PRIuPTR ": pop\n", instr_ptr);
            ++ instr_ptr;
            break;

        case INSTR_RET:
            fprintf(stream, "%6" PRIuPTR ": ret\n", instr_ptr);
            ++ instr_ptr;
            break;

        default:
            fprintf(stream, "%6" PRIuPTR ": illegal instruction %d\n", instr_ptr, instrs[instr_ptr]);
            ++ instr_ptr;
            break;
        }
    }
}
