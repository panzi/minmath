#include "testdata.h"
#include "parser.h"
#include "alt_parser.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#define TS_TO_TV(TS) (struct timeval){ .tv_sec = (TS).tv_sec, .tv_usec = (TS).tv_nsec / 1000 }
#define ITERS 100000

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
    int status = 0;

    for (const struct ParseFunc *func = PARSE_FUNCS; func->name; ++ func) {
        printf("Testing %s\n", func->name);
        for (const struct TestCase *test = TESTS; test->expr; ++ test) {
            struct AstNode *expr = func->parse(test->expr, &error);
            if (expr == NULL) {
                if (test->parse_ok) {
                    fprintf(stderr, "*** [%s] Error parsing expression: %s\n", func->name, test->expr);
                    print_parser_error(stderr, test->expr, &error, 1);
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
    printf("recursive descent: %zd.%06zu\n", (ssize_t)tv_bench.tv_sec, (size_t)tv_bench.tv_usec);
    printf("pratt:             %zd.%06zu\n", (ssize_t)tv_alt_bench.tv_sec, (size_t)tv_alt_bench.tv_usec);

    return status;
}
