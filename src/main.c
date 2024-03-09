// #include "parser.h"
#include "fast_parser.h"
#include "optimizer.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <EXPRESSION>...\n", argc > 0 ? argv[0] : "minmath");
        return 1;
    }
    int status = 0;
    struct AstBuffer buffer = AST_BUFFER_INIT();
    for (int argind = 1; argind < argc; ++ argind) {
        struct AstNode *expr;
        struct ErrorInfo error;
        const char *source = argv[argind];

#if 0
        expr = parse_expression_from_string(source, &error);
        if (expr != NULL) {
            int value = ast_execute_with_environ(expr);
            printf("%s = %d\n", source, value);
        } else {
            print_parser_error(stderr, source, &error, 3);
            status = 1;
        }
#endif

        expr = fast_parse(&buffer, source, &error);
        if (expr != NULL) {
            int value = ast_execute_with_environ(expr);
            printf("%s = %d\n", source, value);

            struct AstNode *opt_expr = ast_optimize(&buffer, expr);
            if (opt_expr != NULL) {
                int value = ast_execute_with_environ(opt_expr);
                ast_print(stdout, opt_expr);
                printf(" = %d\n", value);
            } else {
                print_parser_error(stderr, source, &error, 3);
                status = 1;
            }
        } else {
            print_parser_error(stderr, source, &error, 3);
            status = 1;
        }
        ast_buffer_clear(&buffer);
    }
    ast_buffer_free(&buffer);
    return status;
}
