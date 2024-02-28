#include "parser.h"
#include "alt_parser.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <EXPRESSION>...\n", argc > 0 ? argv[0] : "minmath");
        return 1;
    }
    int status = 0;
    for (int argind = 1; argind < argc; ++ argind) {
        struct AstNode *expr;
        struct ErrorInfo error;
        const char *source = argv[argind];

        expr = parse_expression_from_string(source, &error);
        if (expr != NULL) {
            int value = ast_execute(expr);
            printf("%s = %d\n", source, value);
            ast_free(expr);
        } else {
            print_parser_error(stderr, source, &error, 3);
            status = 1;
        }

        expr = alt_parse_expression_from_string(source, &error);
        if (expr != NULL) {
            int value = ast_execute(expr);
            printf("%s = %d\n", source, value);
            ast_free(expr);
        } else {
            print_parser_error(stderr, source, &error, 3);
            status = 1;
        }
    }
    return status;
}
