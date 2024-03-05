#include "testdata.h"
#include "parser.h"
#include "fast_parser.h"
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
#include <inttypes.h>

#define TS_TO_TV(TS) (struct timeval){ .tv_sec = (TS).tv_sec, .tv_usec = (TS).tv_nsec / 1000 }
#define TS_TO_DBL(TS) ((double)(TS).tv_sec + (double)(TS).tv_nsec / 1000000000.0)
#define ITERS 100000

extern char **environ;

#define MAX(x, y) ((x) > (y) ? (x) : (y))

struct OptItem {
    struct AstNode *expr;
    struct AstNode *opt_expr;
    struct Bytecode unopt_bytecode;
    struct Bytecode bytecode;
    struct Bytecode opt_bytecode;
    int *unopt_params;
    int *params;
    struct Param *ast_params;
    size_t ast_params_size;
};

struct ParseFunc {
    const char *name;
    struct AstNode *(*parse)(const char *input, struct ErrorInfo *error);
};

struct Stats {
    struct timespec min;
    struct timespec max;
    struct timespec median;
    struct timespec avg;
    struct timespec sum;
};

const struct ParseFunc PARSE_FUNCS[] = {
    { "Recursive Descent", parse },
    { "Pratt", fast_parse },
    { NULL, NULL },
};

static void opt_item_free(struct OptItem *opt_item);
static void opt_items_free(struct OptItem *opt_items, size_t count);

static bool params_from_environ(const struct Bytecode *bytecode, int *params, char * const *environ);

static struct Param *ast_params_from_environ(char * const *environ);
static size_t ast_params_len(const struct Param *params);
static void ast_params_free(struct Param *params);

static inline struct timespec timespec_add(const struct timespec lhs, const struct timespec rhs);
static inline struct timespec timespec_sub(const struct timespec lhs, const struct timespec rhs);
static inline struct timespec timespec_div(const struct timespec ts, size_t dividend);
static inline struct timespec timespec_middle(const struct timespec *times, size_t nmemb);
static inline void timespec_sort(struct timespec *times, size_t nmemb);
static int timespec_cmp(const void *lhs, const void *rhs);
static inline struct timespec timespec_max(const struct timespec lhs, const struct timespec rhs);

static struct Stats make_stats(struct timespec *times, size_t time_count);
static void print_bench_header();
static void print_bench(const char *name, const struct Stats *stats, struct timespec max_sum);
#define TS_ZERO (struct timespec){ .tv_sec = 0, .tv_nsec = 0, }

struct timespec timespec_sub(const struct timespec lhs, const struct timespec rhs) {
    struct timespec result = {
        .tv_sec  = lhs.tv_sec  - rhs.tv_sec,
        .tv_nsec = lhs.tv_nsec - rhs.tv_nsec,
    };

    if (result.tv_nsec < 0) {
      -- result.tv_sec;
      result.tv_nsec += 1000000000;
    }

    return result;
}

struct timespec timespec_add(const struct timespec lhs, const struct timespec rhs) {
    struct timespec result = {
        .tv_sec  = lhs.tv_sec  + rhs.tv_sec,
        .tv_nsec = lhs.tv_nsec + rhs.tv_nsec,
    };

    if (result.tv_nsec >= 1000000000) {
        ++ result.tv_sec;
        result.tv_nsec -= 1000000000;
    }

    return result;
}

// not thought through for negative values
struct timespec timespec_div(const struct timespec ts, size_t dividend) {
    struct timespec half = {
        .tv_sec  = ts.tv_sec  / dividend,
        .tv_nsec = ts.tv_nsec / dividend,
    };

    half.tv_nsec += (ts.tv_sec - (half.tv_sec * dividend)) * 1000000000/dividend;
    assert(half.tv_nsec < 1000000000);

    return half;
}

struct timespec timespec_middle(const struct timespec *times, size_t nmemb) {
    assert(nmemb > 0);
    if (nmemb == 0) {
        return (struct timespec){
            .tv_sec  = 0,
            .tv_nsec = 0,
        };
    } else if (nmemb % 2 == 0) {
        struct timespec sum = timespec_add(times[nmemb / 2], times[nmemb / 2 + 1]);
        return timespec_div(sum, 2);
    } else {
        return times[nmemb / 2];
    }
}

void timespec_sort(struct timespec *times, size_t nmemb) {
    qsort(times, nmemb, sizeof(struct timespec), timespec_cmp);
}

int timespec_cmp(const void *lhs, const void *rhs) {
    const struct timespec *lts = lhs;
    const struct timespec *rts = rhs;
    return (
        lts->tv_sec  < rts->tv_sec  ? -1 :
        lts->tv_sec  > rts->tv_sec  ?  1 :
        lts->tv_nsec < rts->tv_nsec ? -1 :
        lts->tv_nsec > rts->tv_nsec ?  1 :
        0
    );
}

struct timespec timespec_max(const struct timespec lhs, const struct timespec rhs) {
    if (lhs.tv_sec > rhs.tv_sec) {
        return lhs;
    } else if (rhs.tv_sec > lhs.tv_sec || rhs.tv_nsec > lhs.tv_nsec) {
        return rhs;
    } else {
        return lhs;
    }
}

struct Stats make_stats(struct timespec *times, size_t time_count) {
    assert(time_count > 0);
    struct timespec ts_sum = TS_ZERO;
    for (size_t index = 0; index < time_count; ++ index) {
        ts_sum = timespec_add(ts_sum, times[index]);
    }

    timespec_sort(times, time_count);

    struct timespec ts_min = times[0];
    struct timespec ts_max = times[time_count - 1];
    struct timespec ts_median = timespec_middle(times, time_count);
    struct timespec ts_avg = timespec_div(ts_sum, time_count);

    return (struct Stats){
        .min = ts_min,
        .max = ts_max,
        .median = ts_median,
        .avg = ts_avg,
        .sum = ts_sum,
    };
}

void print_bench_header() {
    printf("                   %-16s  %-16s  %-16s  %-16s  %-16s\n", "sum", "min", "max", "avg", "median");
}

void print_bench(const char *name, const struct Stats *stats, struct timespec max_sum) {
    double dbl_sum = TS_TO_DBL(stats->sum);
    double dbl_max_sum = TS_TO_DBL(max_sum);

    size_t name_len = strlen(name);
    int padding = name_len <= 17 ? 17 - (int)name_len : 0;

    printf(
        "%s:%*s %2" PRIi64 ".%09ld sec  %2" PRIi64 ".%09ld sec  %2" PRIi64 ".%09ld sec  %2" PRIi64 ".%09ld sec  %2" PRIi64 ".%09ld sec  %6.2lf %%  %.2lfx\n",
        name, padding, "",
        stats->sum.tv_sec, stats->sum.tv_nsec,
        stats->min.tv_sec, stats->min.tv_nsec,
        stats->max.tv_sec, stats->max.tv_nsec,
        stats->avg.tv_sec, stats->avg.tv_nsec,
        stats->median.tv_sec, stats->median.tv_nsec,
        100.0 * dbl_sum / dbl_max_sum,
        dbl_max_sum / dbl_sum
    );
}

void opt_item_free(struct OptItem *opt_item) {
    ast_free(opt_item->expr);
    ast_free(opt_item->opt_expr);
    bytecode_free(&opt_item->unopt_bytecode);
    bytecode_free(&opt_item->bytecode);
    bytecode_free(&opt_item->opt_bytecode);
    free(opt_item->unopt_params);
    free(opt_item->params);
    ast_params_free(opt_item->ast_params);
}

void opt_items_free(struct OptItem *opt_items, size_t count) {
    for (size_t index = 0; index < count; ++ index) {
        opt_item_free(&opt_items[index]);
    }
    free(opt_items);
}

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

struct Param *ast_params_from_environ(char * const *environ) {
    size_t len = 0;
    for (char * const *ptr = environ; *ptr; ++ ptr) {
        ++ len;
    }

    struct Param *params = calloc(len + 1, sizeof(struct Param));
    if (params == NULL) {
        return NULL;
    }

    for (size_t index = 0; index < len; ++ index) {
        const char *envvar = environ[index];
        char *ptr = strchr(envvar, '=');
        char *name;
        int value;
        if (ptr == NULL) {
            name = strdup(envvar);
            value = 0;
        } else {
            size_t len = ptr - envvar;
            name = malloc(len + 1);
            if (name != NULL) {
                memcpy(name, envvar, len);
                name[len] = 0;
            }
            value = atoi(ptr + 1);
        }

        if (name == NULL) {
            while (index > 0) {
                free((void*)params[index].name);
                -- index;
            }
            free(params);
            return NULL;
        }

        params[index] = (struct Param){
            .name  = name,
            .value = value,
        };
    }

    params_sort(params, len);

    return params;
}

size_t ast_params_len(const struct Param *params) {
    size_t len = 0;
    if (params != NULL) {
        for (; params->name; ++ params) {
            ++ len;
        }
    }
    return len;
}

void ast_params_free(struct Param *params) {
    if (params != NULL) {
        for (struct Param *ptr = params; ptr->name; ++ ptr) {
            free((void*)ptr->name);
        }
        free(params);
    }
}

int main(int argc, char *argv[]) {
    struct ErrorInfo error;
    struct timespec ts_start, ts_end;
    int res_start, res_end;
    size_t error_count = 0;
    struct Bytecode bytecode = BYTECODE_INIT();

    for (const struct ParseFunc *func = PARSE_FUNCS; func->name; ++ func) {
        printf("Testing with %s parser...\n", func->name);
        for (const struct TestCase *test = TESTS; test->expr; ++ test) {
            struct AstNode *expr = func->parse(test->expr, &error);
            if (expr == NULL) {
                fprintf(stderr, "*** [%s] Error parsing expression: \"%s\"\n", func->name, test->expr);
                print_parser_error(stderr, test->expr, &error, 1);
                ++ error_count;
            } else {
                // Test AST interpreter
                char **environ_bakup = environ;
                environ = test->environ;
                int result = ast_execute_with_environ(expr);
                environ = environ_bakup;

                if (result != test->result) {
                    fprintf(stderr, "*** [%s] Result missmatch of ast_execute_with_environ():\nEnvironment:\n", func->name);
                    for (char **ptr = test->environ; *ptr; ++ ptr) {
                        fprintf(stderr, "    %s\n", *ptr);
                    }
                    fprintf(stderr,
                        "Expression:\n    %s\nParsed Expression:\n    ", test->expr);
                    ast_print(stderr, expr);

                    if (func->parse != parse) {
                        struct AstNode *rd_expr = parse(test->expr, NULL);
                        if (rd_expr != NULL) {
                            fprintf(stderr, "\nRD Parser:\n    ");
                            ast_print(stderr, rd_expr);
                            ast_free(rd_expr);
                        }
                    }

                    fprintf(stderr, "\nResult:\n    %d\nExpected:\n    %d\n\n",
                        result, test->result);

                    ++ error_count;
                }

                struct Param *ast_params = ast_params_from_environ(test->environ);
                size_t ast_params_size = ast_params_len(ast_params);
                if (ast_params == NULL) {
                    fprintf(stderr, "[%s] Error creating ast params: %s\n", func->name, strerror(errno));
                    ++ error_count;
                } else {
                    int result = ast_execute_with_params(expr, ast_params, ast_params_size);

                    if (result != test->result) {
                        fprintf(stderr, "*** [%s] Result missmatch of ast_execute_with_params():\nParameters:\n", func->name);
                        for (const struct Param *param = ast_params; param->name; ++ param) {
                            fprintf(stderr, "    %s = %d\n", param->name, param->value);
                        }
                        fprintf(stderr,
                            "Expression:\n    %s\nParsed Expression:\n    ", test->expr);
                        ast_print(stderr, expr);
                        fprintf(stderr, "\nResult:\n    %d\nExpected:\n    %d\n\n",
                            result, test->result);

                        ++ error_count;
                    }
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
                    int result = ast_execute_with_environ(opt_expr);
                    environ = environ_bakup;
                    if (result != test->result) {
                        fprintf(stderr, "*** [%s] Optimized result missmatch of ast_execute_with_environ():\nEnvironment:\n", func->name);
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

                    if (ast_params != NULL) {
                        int result = ast_execute_with_params(expr, ast_params, ast_params_size);

                        if (result != test->result) {
                            fprintf(stderr, "*** [%s] Optimized result missmatch of ast_execute_with_params():\nParameters:\n", func->name);
                            for (const struct Param *param = ast_params; param->name; ++ param) {
                                fprintf(stderr, "    %s = %d\n", param->name, param->value);
                            }
                            fprintf(stderr,
                                "Expression:\n    %s\nParsed Expression:\n    ", test->expr);
                            ast_print(stderr, expr);
                            fprintf(stderr, "\nResult:\n    %d\nExpected:\n    %d\n\n",
                                result, test->result);

                            ++ error_count;
                        }
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

                ast_params_free(ast_params);
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

    size_t test_count = 0;
    for (const struct TestCase *test = TESTS; test->expr; ++ test) {
        ++ test_count;
    }

    printf("\nBenchmarking parsing with %d iterations per expression:\n\n", ITERS);

#define PARSER_COUNT 2
#define INDEX_SLOW_PARSER 0
#define INDEX_FAST_PARSER 1

    struct timespec *parse_times = calloc(PARSER_COUNT * ITERS * test_count, sizeof(struct timespec));
    if (parse_times == NULL) {
        perror("calloc(PARSER_COUNT * ITERS * test_count, sizeof(struct timespec))");
        return 1;
    }

    for (size_t iter = 0; iter < ITERS; ++ iter) {
        res_start = clock_gettime(CLOCK_MONOTONIC, &ts_start);
        for (const struct TestCase *test = TESTS; test->expr; ++ test) {
            struct AstNode *expr = parse(test->expr, &error);
            if (expr == NULL) {
                fprintf(stderr, "*** Error parsing expression: %s\n", test->expr);
                print_parser_error(stderr, test->expr, &error, 1);
                free(parse_times);
                return 1;
            } else {
                ast_free(expr);
            }
        }
        res_end = clock_gettime(CLOCK_MONOTONIC, &ts_end);
        assert(res_start == 0); (void)res_start;
        assert(res_end == 0); (void)res_end;
        parse_times[INDEX_SLOW_PARSER * ITERS + iter] = timespec_sub(ts_end, ts_start);
    }

    for (size_t iter = 0; iter < ITERS; ++ iter) {
        res_start = clock_gettime(CLOCK_MONOTONIC, &ts_start);
        for (const struct TestCase *test = TESTS; test->expr; ++ test) {
            struct AstNode *expr = fast_parse(test->expr, &error);
            if (expr == NULL) {
                fprintf(stderr, "*** Error parsing expression: %s\n", test->expr);
                print_parser_error(stderr, test->expr, &error, 1);
                free(parse_times);
                return 1;
            } else {
                ast_free(expr);
            }
        }
        res_end = clock_gettime(CLOCK_MONOTONIC, &ts_end);
        assert(res_start == 0); (void)res_start;
        assert(res_end == 0); (void)res_end;
        parse_times[INDEX_FAST_PARSER * ITERS + iter] = timespec_sub(ts_end, ts_start);
    }

    struct Stats slow_parser_stats = make_stats(parse_times + INDEX_SLOW_PARSER * ITERS, ITERS);
    struct Stats fast_parser_stats = make_stats(parse_times + INDEX_FAST_PARSER * ITERS, ITERS);

    struct timespec parser_max_sum = timespec_max(slow_parser_stats.sum, fast_parser_stats.sum);

    printf("Parser benchmark result:\n");
    print_bench_header();
    print_bench("Recursive Descent", &slow_parser_stats, parser_max_sum);
    print_bench("Pratt",             &fast_parser_stats, parser_max_sum);

    free(parse_times);

    printf("\nBenchmarking execution with %d iterations per expression:\n\n", ITERS);

    // Benchmarking optimizations
    struct OptItem *opt_items = calloc(test_count, sizeof(struct OptItem));
    if (opt_items == NULL) {
        perror("calloc(test_count, sizeof(struct OptItem))");
        return 1;
    }

    size_t max_stack_size = 0;
    for (size_t index = 0; index < test_count; ++ index) {
        struct OptItem *opt_item = &opt_items[index];
        const struct TestCase *test = &TESTS[index];

        opt_item->expr = fast_parse(test->expr, NULL);
        if (opt_item->expr == NULL) {
            perror("fast_parse(test->expr, NULL)");
            goto opt_init_loop_error;
        }

        opt_item->opt_expr = ast_optimize(opt_item->expr);
        if (opt_item->opt_expr == NULL) {
            perror("ast_optimize(test->expr)");
            goto opt_init_loop_error;
        }

        if (!bytecode_compile(&opt_item->unopt_bytecode, opt_item->expr)) {
            perror("bytecode_compile(&opt_item->unopt_bytecode, opt_item->expr)");
            goto opt_init_loop_error;
        }

        if (!bytecode_compile(&opt_item->bytecode, opt_item->opt_expr)) {
            perror("bytecode_compile(&opt_item->bytecode, opt_item->opt_expr)");
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

        opt_item->unopt_params = bytecode_alloc_params(&opt_item->unopt_bytecode);
        if (opt_item->unopt_params == NULL) {
            perror("bytecode_alloc_params(&opt_item->unopt_bytecode)");
            goto opt_init_loop_error;
        }

        opt_item->params = bytecode_alloc_params(&opt_item->bytecode);
        if (opt_item->params == NULL) {
            perror("bytecode_alloc_params(&opt_item->bytecode)");
            goto opt_init_loop_error;
        }

        if (opt_item->unopt_bytecode.stack_size > max_stack_size) {
            max_stack_size = opt_item->unopt_bytecode.stack_size;
        }

        if (opt_item->bytecode.stack_size > max_stack_size) {
            max_stack_size = opt_item->bytecode.stack_size;
        }

        if (opt_item->opt_bytecode.stack_size > max_stack_size) {
            max_stack_size = opt_item->opt_bytecode.stack_size;
        }

        if (!params_from_environ(&opt_item->unopt_bytecode, opt_item->unopt_params, test->environ)) {
            perror("params_from_environ(&opt_item->unopt_bytecode, opt_item->unopt_params, test->environ)");
            goto opt_init_loop_error;
        }

        if (!params_from_environ(&opt_item->bytecode, opt_item->params, test->environ)) {
            perror("params_from_environ(&opt_item->bytecode, opt_item->params, test->environ)");
            goto opt_init_loop_error;
        }

        opt_item->ast_params = ast_params_from_environ(test->environ);
        if (opt_item->ast_params == NULL) {
            perror("ast_params_from_environ(test->environ)");
            goto opt_init_loop_error;
        }
        opt_item->ast_params_size = ast_params_len(opt_item->ast_params);

        continue;
    opt_init_loop_error:
        opt_items_free(opt_items, index + 1);
        return 1;
    }

    int *stack = calloc(max_stack_size, sizeof(int));
    if (stack == NULL) {
        perror("calloc(max_stack_size, sizeof(int))");
        opt_items_free(opt_items, test_count);
        return 1;
    }

#define BENCH_COUNT 7
#define INDEX_AST_EXECUTE                 0
#define INDEX_OPT_AST_EXECUTE             1
#define INDEX_AST_EXECUTE_WITH_PARAMS     2
#define INDEX_OPT_AST_EXECUTE_WITH_PARAMS 3
#define INDEX_UNOPT_BYTECODE_EXECUTE      4
#define INDEX_BYTECODE_EXECUTE            5
#define INDEX_OPT_BYTECODE_EXECUTE        6

    struct timespec *exec_times = calloc(ITERS * BENCH_COUNT, sizeof(struct timespec));
    if (exec_times == NULL) {
        perror("calloc(test_count * ITERS * BENCH_COUNT, sizeof(struct timespec))");
        opt_items_free(opt_items, test_count);
        free(stack);
        return 1;
    }

    // ast_execute_with_environ()
    for (size_t iter = 0; iter < ITERS; ++ iter) {
        char **environ_bakup = environ;
        res_start = clock_gettime(CLOCK_MONOTONIC, &ts_start);
        for (size_t test_index = 0; test_index < test_count; ++ test_index) {
            const struct TestCase *test = &TESTS[test_index];
            struct OptItem *opt_item = &opt_items[test_index];

            environ = test->environ;
            int result = ast_execute_with_environ(opt_item->expr);
            environ = environ_bakup;

            if (result != test->result) {
                fprintf(stderr, "%zu: %s -> %d != %d\n", test_index, test->expr, result, test->result);
                opt_items_free(opt_items, test_count);
                free(stack);
                free(exec_times);
                return 1;
            }
        }
        res_end = clock_gettime(CLOCK_MONOTONIC, &ts_end);
        assert(res_start == 0); (void)res_start;
        assert(res_end == 0); (void)res_end;
        exec_times[INDEX_AST_EXECUTE * ITERS + iter] = timespec_sub(ts_end, ts_start);
    }

    // ast_optimize() + ast_execute_with_environ()
    for (size_t test_index = 0; test_index < test_count; ++ test_index) {
        const struct TestCase *test = &TESTS[test_index];
        struct OptItem *opt_item = &opt_items[test_index];

        for (size_t iter = 0; iter < ITERS; ++ iter) {
            char **environ_bakup = environ;
            res_start = clock_gettime(CLOCK_MONOTONIC, &ts_start);
            environ = test->environ;
            int result = ast_execute_with_environ(opt_item->opt_expr);
            environ = environ_bakup;
            res_end = clock_gettime(CLOCK_MONOTONIC, &ts_end);
            assert(res_start == 0); (void)res_start;
            assert(res_end == 0); (void)res_end;
            struct timespec dur = exec_times[INDEX_OPT_AST_EXECUTE * test_count * ITERS + test_index * ITERS + iter] = timespec_sub(ts_end, ts_start);
            ts_opt_ast_execute = timespec_add(ts_opt_ast_execute, dur);

            if (result != test->result) {
                fprintf(stderr, "%zu: %s -> %d != %d\n", test_index, test->expr, result, test->result);
                opt_items_free(opt_items, test_count);
                free(stack);
                free(exec_times);
                return 1;
            }
        }
    }

    // ast_execute_with_params()
    struct timespec ts_ast_execute_with_params = TS_ZERO;
    for (size_t test_index = 0; test_index < test_count; ++ test_index) {
        const struct TestCase *test = &TESTS[test_index];
        struct OptItem *opt_item = &opt_items[test_index];

        for (size_t iter = 0; iter < ITERS; ++ iter) {
            res_start = clock_gettime(CLOCK_MONOTONIC, &ts_start);
            int result = ast_execute_with_params(opt_item->expr, opt_item->ast_params, opt_item->ast_params_size);
            res_end = clock_gettime(CLOCK_MONOTONIC, &ts_end);
            assert(res_start == 0); (void)res_start;
            assert(res_end == 0); (void)res_end;
            struct timespec dur = exec_times[INDEX_AST_EXECUTE_WITH_PARAMS * test_count * ITERS + test_index * ITERS + iter] = timespec_sub(ts_end, ts_start);
            ts_ast_execute_with_params = timespec_add(ts_ast_execute_with_params, dur);

            if (result != test->result) {
                fprintf(stderr, "%zu: %s -> %d != %d\n", test_index, test->expr, result, test->result);
                opt_items_free(opt_items, test_count);
                free(stack);
                free(exec_times);
                return 1;
            }
        }
    }

    // ast_optimize() + ast_execute_with_environ()
    struct timespec ts_opt_ast_execute_with_params = TS_ZERO;
    for (size_t test_index = 0; test_index < test_count; ++ test_index) {
        const struct TestCase *test = &TESTS[test_index];
        struct OptItem *opt_item = &opt_items[test_index];

        for (size_t iter = 0; iter < ITERS; ++ iter) {
            res_start = clock_gettime(CLOCK_MONOTONIC, &ts_start);
            int result = ast_execute_with_params(opt_item->opt_expr, opt_item->ast_params, opt_item->ast_params_size);
            res_end = clock_gettime(CLOCK_MONOTONIC, &ts_end);
            assert(res_start == 0); (void)res_start;
            assert(res_end == 0); (void)res_end;
            struct timespec dur = exec_times[INDEX_OPT_AST_EXECUTE_WITH_PARAMS * test_count * ITERS + test_index * ITERS + iter] = timespec_sub(ts_end, ts_start);
            ts_opt_ast_execute_with_params = timespec_add(ts_opt_ast_execute_with_params, dur);

            if (result != test->result) {
                fprintf(stderr, "%zu: %s -> %d != %d\n", test_index, test->expr, result, test->result);
                opt_items_free(opt_items, test_count);
                free(stack);
                free(exec_times);
                return 1;
            }
        }
    }

    // bytecode_execute()
    struct timespec ts_unopt_bytecode_execute = TS_ZERO;
    for (size_t test_index = 0; test_index < test_count; ++ test_index) {
        const struct TestCase *test = &TESTS[test_index];
        struct OptItem *opt_item = &opt_items[test_index];

        for (size_t iter = 0; iter < ITERS; ++ iter) {
            res_start = clock_gettime(CLOCK_MONOTONIC, &ts_start);
            int result = bytecode_execute(&opt_item->unopt_bytecode, opt_item->unopt_params, stack);
            res_end = clock_gettime(CLOCK_MONOTONIC, &ts_end);
            assert(res_start == 0); (void)res_start;
            assert(res_end == 0); (void)res_end;
            struct timespec dur = exec_times[INDEX_UNOPT_BYTECODE_EXECUTE * test_count * ITERS + test_index * ITERS + iter] = timespec_sub(ts_end, ts_start);
            ts_unopt_bytecode_execute = timespec_add(ts_unopt_bytecode_execute, dur);

            if (result != test->result) {
                fprintf(stderr, "%zu: %s -> %d != %d\n", test_index, test->expr, result, test->result);
                opt_items_free(opt_items, test_count);
                free(stack);
                free(exec_times);
                return 1;
            }
        }
    }

    // ast_optimize() + bytecode_execute()
    struct timespec ts_bytecode_execute = TS_ZERO;
    for (size_t test_index = 0; test_index < test_count; ++ test_index) {
        const struct TestCase *test = &TESTS[test_index];
        struct OptItem *opt_item = &opt_items[test_index];

        for (size_t iter = 0; iter < ITERS; ++ iter) {
            res_start = clock_gettime(CLOCK_MONOTONIC, &ts_start);
            int result = bytecode_execute(&opt_item->bytecode, opt_item->params, stack);
            res_end = clock_gettime(CLOCK_MONOTONIC, &ts_end);
            assert(res_start == 0); (void)res_start;
            assert(res_end == 0); (void)res_end;
            struct timespec dur = exec_times[INDEX_BYTECODE_EXECUTE * test_count * ITERS + test_index * ITERS + iter] = timespec_sub(ts_end, ts_start);
            ts_bytecode_execute = timespec_add(ts_bytecode_execute, dur);

            if (result != test->result) {
                fprintf(stderr, "%zu: %s -> %d != %d\n", test_index, test->expr, result, test->result);
                opt_items_free(opt_items, test_count);
                free(stack);
                free(exec_times);
                return 1;
            }
        }
    }

    // ast_optimize() + bytecode_optimize() + bytecode_execute()
    struct timespec ts_opt_bytecode_execute = TS_ZERO;
    for (size_t test_index = 0; test_index < test_count; ++ test_index) {
        const struct TestCase *test = &TESTS[test_index];
        struct OptItem *opt_item = &opt_items[test_index];

        for (size_t iter = 0; iter < ITERS; ++ iter) {
            res_start = clock_gettime(CLOCK_MONOTONIC, &ts_start);
            int result = bytecode_execute(&opt_item->opt_bytecode, opt_item->params, stack);
            res_end = clock_gettime(CLOCK_MONOTONIC, &ts_end);
            assert(res_start == 0); (void)res_start;
            assert(res_end == 0); (void)res_end;
            struct timespec dur = exec_times[INDEX_OPT_BYTECODE_EXECUTE * test_count * ITERS + test_index * ITERS + iter] = timespec_sub(ts_end, ts_start);
            ts_opt_bytecode_execute = timespec_add(ts_opt_bytecode_execute, dur);

            if (result != test->result) {
                fprintf(stderr, "%zu: %s -> %d != %d\n", test_index, test->expr, result, test->result);
                opt_items_free(opt_items, test_count);
                free(stack);
                free(exec_times);
                return 1;
            }
        }
    }

    opt_items_free(opt_items, test_count);
    free(stack);

    for (size_t bench_index = 0; bench_index < BENCH_COUNT; ++ bench_index) {
        for (size_t test_index = 0; test_index < test_count; ++ test_index) {
            timespec_sort(exec_times + bench_index * test_count * ITERS + test_index * ITERS, ITERS);
        }
    }

    double dbl_ast_execute                 = TS_TO_DBL(ts_ast_execute);
    double dbl_opt_ast_execute             = TS_TO_DBL(ts_opt_ast_execute);
    double dbl_ast_execute_with_params     = TS_TO_DBL(ts_ast_execute_with_params);
    double dbl_opt_ast_execute_with_params = TS_TO_DBL(ts_opt_ast_execute_with_params);
    double dbl_unopt_bytecode_execute      = TS_TO_DBL(ts_unopt_bytecode_execute);
    double dbl_bytecode_execute            = TS_TO_DBL(ts_bytecode_execute);
    double dbl_opt_bytecode_execute        = TS_TO_DBL(ts_opt_bytecode_execute);

    printf("Execution benchmark result:\n");
    printf("ast with environ:                 %zd.%09zu sec  %9.6lf msec/iters  %9.6lf msec/tests  %6.2lf %%  %.2lfx\n", (ptrdiff_t)ts_ast_execute.tv_sec,                 (size_t)ts_ast_execute.tv_nsec,            dbl_ast_execute                 * 1000 / ITERS, dbl_ast_execute                 * 1000 / test_count, 100.0 * dbl_ast_execute                 / dbl_ast_execute, dbl_ast_execute / dbl_ast_execute                );
    printf("optimized ast with environ:       %zd.%09zu sec  %9.6lf msec/iters  %9.6lf msec/tests  %6.2lf %%  %.2lfx\n", (ptrdiff_t)ts_opt_ast_execute.tv_sec,             (size_t)ts_opt_ast_execute.tv_nsec,        dbl_opt_ast_execute             * 1000 / ITERS, dbl_opt_ast_execute             * 1000 / test_count, 100.0 * dbl_opt_ast_execute             / dbl_ast_execute, dbl_ast_execute / dbl_opt_ast_execute            );
    printf("ast with params:                  %zd.%09zu sec  %9.6lf msec/iters  %9.6lf msec/tests  %6.2lf %%  %.2lfx\n", (ptrdiff_t)ts_ast_execute_with_params.tv_sec,     (size_t)ts_ast_execute.tv_nsec,            dbl_ast_execute_with_params     * 1000 / ITERS, dbl_ast_execute_with_params     * 1000 / test_count, 100.0 * dbl_ast_execute_with_params     / dbl_ast_execute, dbl_ast_execute / dbl_ast_execute_with_params    );
    printf("optimized ast with params:        %zd.%09zu sec  %9.6lf msec/iters  %9.6lf msec/tests  %6.2lf %%  %.2lfx\n", (ptrdiff_t)ts_opt_ast_execute_with_params.tv_sec, (size_t)ts_opt_ast_execute.tv_nsec,        dbl_opt_ast_execute_with_params * 1000 / ITERS, dbl_opt_ast_execute_with_params * 1000 / test_count, 100.0 * dbl_opt_ast_execute_with_params / dbl_ast_execute, dbl_ast_execute / dbl_opt_ast_execute_with_params);
    printf("bytecode:                         %zd.%09zu sec  %9.6lf msec/iters  %9.6lf msec/tests  %6.2lf %%  %.2lfx\n", (ptrdiff_t)ts_unopt_bytecode_execute.tv_sec,      (size_t)ts_unopt_bytecode_execute.tv_nsec, dbl_unopt_bytecode_execute      * 1000 / ITERS, dbl_unopt_bytecode_execute      * 1000 / test_count, 100.0 * dbl_unopt_bytecode_execute      / dbl_ast_execute, dbl_ast_execute / dbl_unopt_bytecode_execute     );
    printf("optimized ast+bytecode:           %zd.%09zu sec  %9.6lf msec/iters  %9.6lf msec/tests  %6.2lf %%  %.2lfx\n", (ptrdiff_t)ts_bytecode_execute.tv_sec,            (size_t)ts_bytecode_execute.tv_nsec,       dbl_bytecode_execute            * 1000 / ITERS, dbl_bytecode_execute            * 1000 / test_count, 100.0 * dbl_bytecode_execute            / dbl_ast_execute, dbl_ast_execute / dbl_bytecode_execute           );
    printf("optimized ast+optimized bytecode: %zd.%09zu sec  %9.6lf msec/iters  %9.6lf msec/tests  %6.2lf %%  %.2lfx\n", (ptrdiff_t)ts_opt_bytecode_execute.tv_sec,        (size_t)ts_opt_bytecode_execute.tv_nsec,   dbl_opt_bytecode_execute        * 1000 / ITERS, dbl_opt_bytecode_execute        * 1000 / test_count, 100.0 * dbl_opt_bytecode_execute        / dbl_ast_execute, dbl_ast_execute / dbl_opt_bytecode_execute       );

    free(exec_times);
#endif
    return 0;
}
