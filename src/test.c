#include "testdata.h"
#include "parser.h"
#include "alt_parser.h"
#include "optimizer.h"
#include "bytecode.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>

#define TS_TO_TV(TS) (struct timeval){ .tv_sec = (TS).tv_sec, .tv_usec = (TS).tv_nsec / 1000 }
#define ITERS 10000

extern char **environ;

#define MAX(x, y) ((x) > (y) ? (x) : (y))

struct OptItem {
    struct AstNode *expr;
    struct AstNode *opt_expr;
    struct Bytecode bytecode;
    struct Bytecode opt_bytecode;
    int *params;
    int *stack;
};

struct ParseFunc {
    const char *name;
    struct AstNode *(*parse)(const char *input, struct ErrorInfo *error);
};

const struct ParseFunc PARSE_FUNCS[] = {
    { "recursive descent", parse },
    { "pratt", alt_parse },
    { NULL, NULL },
};

bool params_from_environ(const struct Bytecode *bytecode, int *params, char * const *environ) {
    char *name = NULL;
    size_t name_size = 0;
    for (char * const *envvar = environ; *envvar; ++ envvar) {
        const char *equals_ptr = strchr(*envvar, '=');
        if (equals_ptr == NULL) {
            errno = EINVAL;
            free(name);
            return false;
        }

        size_t var_size = (size_t)(equals_ptr - *envvar) + 1;
        if (var_size > name_size) {
            char *new_name = realloc(name, var_size);
            if (new_name == NULL) {
                free(name);
                return false;
            }
            name = new_name;
            name_size = var_size;
        }
        memcpy(name, *envvar, var_size - 1);
        name[var_size - 1] = 0;

        char *endptr = NULL;
        const char *valptr = equals_ptr + 1;
        long value = strtol(valptr, &endptr, 10);
        if (!*valptr || *endptr) {
            free(name);
            return false;
        }

        if (value > INT_MAX) {
            errno = ERANGE;
            free(name);
            return false;
        }

        // ignoring return value because parameter might be optimized out
        bytecode_set_param(bytecode, params, name, value);
    }

    free(name);
    return true;
}

int main(int argc, char *argv[]) {
    struct ErrorInfo error;
    struct timespec ts_start, ts_end;
    struct timeval tv_start, tv_end;
    int res_start, res_end;
    size_t error_count = 0;
    struct Bytecode bytecode = BYTECODE_INIT();

    for (const struct ParseFunc *func = PARSE_FUNCS; func->name; ++ func) {
        printf("Testing %s\n", func->name);
        for (const struct TestCase *test = TESTS; test->expr; ++ test) {
            struct AstNode *expr = func->parse(test->expr, &error);
            if (expr == NULL) {
                if (test->parse_ok) {
                    fprintf(stderr, "*** [%s] Error parsing expression: \"%s\"\n", func->name, test->expr);
                    print_parser_error(stderr, test->expr, &error, 1);
                    ++ error_count;
                }
            } else {
                if (!test->parse_ok) {
                    fprintf(stderr, "*** [%s] Expected error parsing expression, but was ok: \"%s\"\n", func->name, test->expr);
                    ++ error_count;
                }

                // Test AST interpreter
                char **environ_bakup = environ;
                environ = test->environ;
                int result = ast_execute(expr);
                environ = environ_bakup;

                if (result != test->result) {
                    fprintf(stderr, "*** [%s] Result missmatch:\nEnvironment:\n", func->name);
                    for (char **ptr = test->environ; *ptr; ++ ptr) {
                        fprintf(stderr, "    %s\n", *ptr);
                    }
                    fprintf(stderr,
                        "Expression:\n    %s\nParsed Expression:\n    ", test->expr);
                    ast_print(stderr, expr);

                    if (func->parse != parse) {
                        fprintf(stderr, "\nRD Parser:\n    ");
                        struct AstNode *expr2 = parse(test->expr, NULL);
                        ast_print(stderr, expr2);
                        ast_free(expr2);
                    }

                    fprintf(stderr, "\nResult:\n    %d\nExpected:\n    %d\n\n",
                        result, test->result);

                    ++ error_count;
                }

                // Test bytecode interpreter
                if (!bytecode_compile(&bytecode, expr)) {
                    fprintf(stderr, "*** [%s] Error compiling to bytecode: %s\n", func->name, strerror(errno));
                    fprintf(stderr, "Expression: %s\n", test->expr);
                    ++ error_count;
                } else {
                    int *stack = bytecode_alloc_stack(&bytecode);
                    if (stack == NULL) {
                        fprintf(stderr, "*** [%s] Error allocating stack: %s\n", func->name, strerror(errno));
                        fprintf(stderr, "Expression: %s\n", test->expr);
                        ++ error_count;
                    } else {
                        int *params = bytecode_alloc_params(&bytecode);
                        if (params == NULL) {
                            fprintf(stderr, "*** [%s] Error allocating params: %s\n", func->name, strerror(errno));
                            fprintf(stderr, "Expression: %s\n", test->expr);
                            ++ error_count;
                        } else {
                            if (!params_from_environ(&bytecode, params, test->environ)) {
                                fprintf(stderr, "*** [%s] Error initializing params: %s\n", func->name, strerror(errno));
                                fprintf(stderr, "Expression: %s\nEnvironment:\n", test->expr);
                                for (char **ptr = test->environ; *ptr; ++ ptr) {
                                    fprintf(stderr, "    %s\n", *ptr);
                                }
                                ++ error_count;
                            } else {
                                result = bytecode_execute(&bytecode, params, stack);

                                if (result != test->result) {
                                    fprintf(stderr, "*** [%s] Bytecode execution result missmatch:\nEnvironment:\n", func->name);
                                    for (char **ptr = test->environ; *ptr; ++ ptr) {
                                        fprintf(stderr, "    %s\n", *ptr);
                                    }
                                    fprintf(stderr, "Expression:\n    %s\nBytecode:\n", test->expr);
                                    bytecode_print(&bytecode, stderr);
                                    fprintf(stderr,
                                        "\nResult:\n    %d\nExpected:\n    %d\n\n",
                                        result, test->result);

                                    ++ error_count;
                                }
                            }

                            free(params);
                        }

                        free(stack);
                    }
                }

                bytecode_clear(&bytecode);

                // Optimizations
                struct AstNode *opt_expr = ast_optimize(expr);
                if (opt_expr == NULL) {
                    fprintf(stderr,
                        "*** [%s] Error optimizing expression \"%s\": %s\n",
                        func->name, test->expr, strerror(errno));
                    ++ error_count;
                } else {
                    // Test AST interpreter on optimized AST
                    environ = test->environ;
                    int result = ast_execute(opt_expr);
                    environ = environ_bakup;
                    if (result != test->result) {
                        fprintf(stderr, "*** [%s] Optimized result missmatch:\nEnvironment:\n", func->name);
                        for (char **ptr = test->environ; *ptr; ++ ptr) {
                            fprintf(stderr, "    %s\n", *ptr);
                        }
                        fprintf(stderr,
                            "Expression:\n    %s\nOptimized Expression:\n    ",
                            test->expr);
                        ast_print(stderr, opt_expr);
                        fprintf(stderr,
                            "\nResult:\n    %d\nExpected:\n    %d\n\n",
                            result, test->result);

                        ++ error_count;
                    }

                    // Test optimized bytecode interpreter on optimized AST
                    if (!bytecode_compile(&bytecode, opt_expr)) {
                        fprintf(stderr, "*** [%s] Error compiling to bytecode: %s\n", func->name, strerror(errno));
                        fprintf(stderr, "Expression: %s\n", test->expr);
                        ++ error_count;
                    } else if (!bytecode_optimize(&bytecode)) {
                            fprintf(stderr, "*** [%s] Error optimizing bytecode: %s\n", func->name, strerror(errno));
                            fprintf(stderr, "Expression: %s\n", test->expr);
                            ++ error_count;
                    } else {
                        int *stack = bytecode_alloc_stack(&bytecode);
                        if (stack == NULL) {
                            fprintf(stderr, "*** [%s] Error allocating stack: %s\n", func->name, strerror(errno));
                            fprintf(stderr, "Expression: %s\n", test->expr);
                            ++ error_count;
                        } else {
                            int *params = bytecode_alloc_params(&bytecode);
                            if (params == NULL) {
                                fprintf(stderr, "*** [%s] Error allocating params: %s\n", func->name, strerror(errno));
                                fprintf(stderr, "Expression: %s\n", test->expr);
                                ++ error_count;
                            } else {
                                if (!params_from_environ(&bytecode, params, test->environ)) {
                                    fprintf(stderr, "*** [%s] Error initializing params: %s\n", func->name, strerror(errno));
                                    fprintf(stderr, "Expression: %s\nEnvironment:\n", test->expr);
                                    for (char **ptr = test->environ; *ptr; ++ ptr) {
                                        fprintf(stderr, "    %s\n", *ptr);
                                    }
                                    ++ error_count;
                                } else {
                                    result = bytecode_execute(&bytecode, params, stack);

                                    if (result != test->result) {
                                        fprintf(stderr, "*** [%s] Bytecode execution result missmatch:\nEnvironment:\n", func->name);
                                        for (char **ptr = test->environ; *ptr; ++ ptr) {
                                            fprintf(stderr, "    %s\n", *ptr);
                                        }
                                        fprintf(stderr,
                                            "Expression:\n    %s\nOptimized Expression:\n    ",
                                            test->expr);
                                        ast_print(stderr, opt_expr);
                                        fprintf(stderr, "\nOptimized Bytecode:\n");
                                        bytecode_print(&bytecode, stderr);
                                        fprintf(stderr,
                                            "\nResult:\n    %d\nExpected:\n    %d\n\n",
                                            result, test->result);

                                        ++ error_count;
                                    }
                                }

                                free(params);
                            }

                            free(stack);
                        }
                    }

                    bytecode_clear(&bytecode);
                    ast_free(opt_expr);
                }

                ast_free(expr);
            }

            // if (error_count > 0) {
            //     bytecode_free(&bytecode);
            //     fprintf(stderr, "Aborting due to errors!\n");
            //     return 1;
            // }
        }
    }

    bytecode_free(&bytecode);

    if (error_count > 0) {
        fprintf(stderr, "%zu errors!\n", error_count);
        return 1;
    }

    printf("Benchmarking parsing with %d iterations per expression:\n", ITERS);

    res_start = clock_gettime(CLOCK_MONOTONIC, &ts_start);
    for (const struct TestCase *test = TESTS; test->expr; ++ test) {
        for (size_t iter = 0; iter < ITERS; ++ iter) {
            struct AstNode *expr = parse(test->expr, &error);
            if (expr == NULL) {
                if (test->parse_ok) {
                    fprintf(stderr, "*** Error parsing expression: %s\n", test->expr);
                    print_parser_error(stderr, test->expr, &error, 1);
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
            struct AstNode *expr = alt_parse(test->expr, &error);
            if (expr == NULL) {
                if (test->parse_ok) {
                    fprintf(stderr, "*** Error parsing expression: %s\n", test->expr);
                    print_parser_error(stderr, test->expr, &error, 1);
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

    double dbl_bench     = (double)tv_bench.tv_sec     + (double)tv_bench.tv_usec     / 1000000;
    double dbl_alt_bench = (double)tv_alt_bench.tv_sec + (double)tv_alt_bench.tv_usec / 1000000;

    // TODO: devide by ITERS and number of tests
    printf("Parser benchmark result:\n");
    printf("recursive descent: %zd.%06zu sec  %6.2lf %%\n", (ssize_t)tv_bench.tv_sec,     (size_t)tv_bench.tv_usec,     100.0 * dbl_bench     / dbl_bench);
    printf("pratt:             %zd.%06zu sec  %6.2lf %%\n", (ssize_t)tv_alt_bench.tv_sec, (size_t)tv_alt_bench.tv_usec, 100.0 * dbl_alt_bench / dbl_bench);

    printf("\n");
    printf("pratt is %.2fx as fast\n\n", dbl_bench / dbl_alt_bench);

    printf("Benchmarking execution with %d iterations per expression:\n", ITERS);

    size_t test_count = 0;
    for (const struct TestCase *test = TESTS; test->expr; ++ test) {
        ++ test_count;
    }

    // Benchmarking optimizations
    struct OptItem *opt_items = calloc(test_count, sizeof(struct OptItem));
    if (opt_items == NULL) {
        perror("calloc(test_count, sizeof(struct OptItem))");
        return 1;
    }

    for (size_t index = 0; index < test_count; ++ index) {
        struct OptItem *opt_item = &opt_items[index];
        const struct TestCase *test = &TESTS[index];

        opt_item->expr = alt_parse(test->expr, NULL);
        if (opt_item->expr == NULL) {
            perror("alt_parse(test->expr, NULL)");
            goto opt_init_loop_error;
        }

        opt_item->opt_expr = ast_optimize(opt_item->expr);
        if (opt_item->opt_expr == NULL) {
            perror("ast_optimize(test->expr)");
            goto opt_init_loop_error;
        }

        if (!bytecode_compile(&opt_item->bytecode, opt_item->expr)) {
            perror("bytecode_compile(&opt_item->bytecode, opt_item->expr)");
            goto opt_init_loop_error;
        }

        if (!bytecode_clone(&opt_item->bytecode, &opt_item->opt_bytecode)) {
            perror("bytecode_clone(&opt_item->bytecode, &opt_item->opt_bytecode)");
            goto opt_init_loop_error;
        }

        if (!bytecode_optimize(&opt_item->opt_bytecode)) {
            perror("bytecode_optimize(&opt_item->opt_bytecode)");
            goto opt_init_loop_error;
        }

        opt_item->params = bytecode_alloc_params(&opt_item->bytecode);
        if (opt_item->params == NULL) {
            perror("bytecode_alloc_params(&opt_item->bytecode)");
            goto opt_init_loop_error;
        }

        size_t stack_size = MAX(
            opt_item->bytecode.stack_size,
            opt_item->opt_bytecode.stack_size
        );

        opt_item->stack = calloc(stack_size, sizeof(uint8_t));
        if (opt_item->stack == NULL) {
            perror("calloc(stack_size, sizeof(int))");
            goto opt_init_loop_error;
        }

        if (!params_from_environ(&opt_item->bytecode, opt_item->params, test->environ)) {
            perror("params_from_environ(&opt_item->bytecode, opt_item->params, test->environ)");
            goto opt_init_loop_error;
        }

        continue;
    opt_init_loop_error:
        for (size_t free_index = 0; free_index <= index; ++ free_index) {
            ast_free(opt_item->expr);
            ast_free(opt_item->opt_expr);
            bytecode_free(&opt_item->bytecode);
            bytecode_free(&opt_item->opt_bytecode);
            free(opt_item->params);
            free(opt_item->stack);
        }
        free(opt_items);
        return 1;
    }

    res_start = clock_gettime(CLOCK_MONOTONIC, &ts_start);
    for (size_t index = 0; index < test_count; ++ index) {
        const struct TestCase *test = &TESTS[index];
        struct OptItem *opt_item = &opt_items[index];

        for (size_t iter = 0; iter < ITERS; ++ iter) {
            char **environ_bakup = environ;
            environ = test->environ;
            int result = ast_execute(opt_item->expr);
            environ = environ_bakup;

            if (result != test->result) {
                fprintf(stderr, "%zu: %s -> %d != %d\n", index, test->expr, result, test->result);
                return 1;
            }
        }
    }
    res_end = clock_gettime(CLOCK_MONOTONIC, &ts_end);
    assert(res_start == 0); (void)res_start;
    assert(res_end == 0); (void)res_end;

    tv_start = TS_TO_TV(ts_start);
    tv_end = TS_TO_TV(ts_end);

    struct timeval tv_bench_ast_execute;
    timersub(&tv_end, &tv_start, &tv_bench_ast_execute);

    res_start = clock_gettime(CLOCK_MONOTONIC, &ts_start);
    for (size_t index = 0; index < test_count; ++ index) {
        const struct TestCase *test = &TESTS[index];
        struct OptItem *opt_item = &opt_items[index];

        for (size_t iter = 0; iter < ITERS; ++ iter) {
            char **environ_bakup = environ;
            environ = test->environ;
            int result = ast_execute(opt_item->opt_expr);
            environ = environ_bakup;

            if (result != test->result) {
                fprintf(stderr, "%zu: %s -> %d != %d\n", index, test->expr, result, test->result);
                return 1;
            }
        }
    }
    res_end = clock_gettime(CLOCK_MONOTONIC, &ts_end);
    assert(res_start == 0); (void)res_start;
    assert(res_end == 0); (void)res_end;

    tv_start = TS_TO_TV(ts_start);
    tv_end = TS_TO_TV(ts_end);

    struct timeval tv_bench_opt_ast_execute;
    timersub(&tv_end, &tv_start, &tv_bench_opt_ast_execute);

    res_start = clock_gettime(CLOCK_MONOTONIC, &ts_start);
    for (size_t index = 0; index < test_count; ++ index) {
        const struct TestCase *test = &TESTS[index];
        struct OptItem *opt_item = &opt_items[index];

        for (size_t iter = 0; iter < ITERS; ++ iter) {
            int result = bytecode_execute(&opt_item->bytecode, opt_item->params, opt_item->stack);

            if (result != test->result) {
                fprintf(stderr, "%zu: %s -> %d != %d\n", index, test->expr, result, test->result);
                return 1;
            }
        }
    }
    res_end = clock_gettime(CLOCK_MONOTONIC, &ts_end);
    assert(res_start == 0); (void)res_start;
    assert(res_end == 0); (void)res_end;

    tv_start = TS_TO_TV(ts_start);
    tv_end = TS_TO_TV(ts_end);

    struct timeval tv_bench_bytecode_execute;
    timersub(&tv_end, &tv_start, &tv_bench_bytecode_execute);

    res_start = clock_gettime(CLOCK_MONOTONIC, &ts_start);
    for (size_t index = 0; index < test_count; ++ index) {
        const struct TestCase *test = &TESTS[index];
        struct OptItem *opt_item = &opt_items[index];

        for (size_t iter = 0; iter < ITERS; ++ iter) {
            int result = bytecode_execute(&opt_item->opt_bytecode, opt_item->params, opt_item->stack);

            if (result != test->result) {
                fprintf(stderr, "%zu: %s -> %d != %d\n", index, test->expr, result, test->result);
                return 1;
            }
        }
    }
    res_end = clock_gettime(CLOCK_MONOTONIC, &ts_end);
    assert(res_start == 0); (void)res_start;
    assert(res_end == 0); (void)res_end;

    tv_start = TS_TO_TV(ts_start);
    tv_end = TS_TO_TV(ts_end);

    struct timeval tv_bench_opt_bytecode_execute;
    timersub(&tv_end, &tv_start, &tv_bench_opt_bytecode_execute);

    for (size_t index = 0; index < test_count; ++ index) {
        struct OptItem *opt_item = &opt_items[index];
        ast_free(opt_item->expr);
        ast_free(opt_item->opt_expr);
        bytecode_free(&opt_item->bytecode);
        bytecode_free(&opt_item->opt_bytecode);
        free(opt_item->params);
        free(opt_item->stack);
    }
    free(opt_items);

    double dbl_ast_execute          = (double)tv_bench_ast_execute.tv_sec          + (double)tv_bench_ast_execute.tv_usec          / 1000000;
    double dbl_opt_ast_execute      = (double)tv_bench_opt_ast_execute.tv_sec      + (double)tv_bench_opt_ast_execute.tv_usec      / 1000000;
    double dbl_bytecode_execute     = (double)tv_bench_bytecode_execute.tv_sec     + (double)tv_bench_bytecode_execute.tv_usec     / 1000000;
    double dbl_opt_bytecode_execute = (double)tv_bench_opt_bytecode_execute.tv_sec + (double)tv_bench_opt_bytecode_execute.tv_usec / 1000000;

    printf("Execution benchmark result:\n");
    printf("ast:                %zd.%06zu sec  %6.2lf %%\n", (ssize_t)tv_bench_ast_execute.tv_sec,          (size_t)tv_bench_ast_execute.tv_usec,          100.0 * dbl_ast_execute          / dbl_ast_execute);
    printf("optimized ast:      %zd.%06zu sec  %6.2lf %%\n", (ssize_t)tv_bench_opt_ast_execute.tv_sec,      (size_t)tv_bench_opt_ast_execute.tv_usec,      100.0 * dbl_opt_ast_execute      / dbl_ast_execute);
    printf("bytecode:           %zd.%06zu sec  %6.2lf %%\n", (ssize_t)tv_bench_bytecode_execute.tv_sec,     (size_t)tv_bench_bytecode_execute.tv_usec,     100.0 * dbl_bytecode_execute     / dbl_ast_execute);
    printf("optimized bytecode: %zd.%06zu sec  %6.2lf %%\n", (ssize_t)tv_bench_opt_bytecode_execute.tv_sec, (size_t)tv_bench_opt_bytecode_execute.tv_usec, 100.0 * dbl_opt_bytecode_execute / dbl_ast_execute);


    return 0;
}
