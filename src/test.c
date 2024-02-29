#include "testdata.h"
#include "parser.h"
#include "alt_parser.h"
#include "optimizer.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>

#define TS_TO_TV(TS) (struct timeval){ .tv_sec = (TS).tv_sec, .tv_usec = (TS).tv_nsec / 1000 }
#define ITERS 10000

extern char **environ;

struct ParseFunc {
    const char *name;
    struct AstNode *(*parse)(const char *input, struct ErrorInfo *error);
};

const struct ParseFunc PARSE_FUNCS[] = {
    { "recursive descent", parse_expression_from_string },
    { "pratt", parse_expression_from_string },
    { NULL, NULL },
};

int main(int argc, char *argv[]) {
    struct ErrorInfo error;
    struct timespec ts_start, ts_end;
    struct timeval tv_start, tv_end;
    int res_start, res_end;
    size_t error_count = 0;

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

                char **environ_bakup = environ;
                environ = test->environ;
                // fprintf(stderr, ">>> %s\n", test->expr);
                int result = ast_execute(expr);
                environ = environ_bakup;

                if (result != test->result) {
                    fprintf(stderr, "*** [%s] Result missmatch:\nEnvironment:\n", func->name);
                    for (char **ptr = test->environ; *ptr; ++ ptr) {
                        fprintf(stderr, "    %s\n", *ptr);
                    }
                    fprintf(stderr,
                        "Expression:\n    %s\nResult:\n    %d\nExpected:\n    %d\n\n",
                        test->expr, result, test->result);

                    ++ error_count;
                }

                struct AstNode *opt_expr = ast_optimize(expr);
                if (opt_expr == NULL) {
                    fprintf(stderr,
                        "*** [%s] Error optimizing expression \"%s\": %s\n",
                        func->name, test->expr, strerror(errno));
                    ++ error_count;
                } else {
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
                    ast_free(opt_expr);
                }

                ast_free(expr);
            }
        }
    }

    if (error_count > 0) {
        fprintf(stderr, "%zu errors!\n", error_count);
        return 1;
    }

    printf("Benchmarking parsing with %d iterations per expression:\n", ITERS);

    res_start = clock_gettime(CLOCK_MONOTONIC, &ts_start);
    for (const struct TestCase *test = TESTS; test->expr; ++ test) {
        for (size_t iter = 0; iter < ITERS; ++ iter) {
            struct AstNode *expr = parse_expression_from_string(test->expr, &error);
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
            struct AstNode *expr = alt_parse_expression_from_string(test->expr, &error);
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

    // TODO: devide by ITERS and number of tests
    printf("Benchmark result:\n");
    printf("recursive descent: %zd.%06zu\n", (ssize_t)tv_bench.tv_sec,     (size_t)tv_bench.tv_usec);
    printf("pratt:             %zd.%06zu\n", (ssize_t)tv_alt_bench.tv_sec, (size_t)tv_alt_bench.tv_usec);

    double dbl_bench     = (double)tv_bench.tv_sec     + (double)tv_bench.tv_usec     / 1000000;
    double dbl_alt_bench = (double)tv_alt_bench.tv_sec + (double)tv_alt_bench.tv_usec / 1000000;

    printf("\n");
    printf("pratt is %.2fx as fast\n", dbl_bench / dbl_alt_bench);

    return 0;
}
