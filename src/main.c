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

        expr = parse_expression_from_string(argv[argind]);
        if (expr != NULL) {
            int value = ast_execute(expr);
            printf("%s = %d\n", argv[argind], value);
            ast_free(expr);
        } else {
            fprintf(stderr, "%s = ERROR: %s\n", argv[argind], strerror(errno));
            status = 1;
        }

        expr = alt_parse_expression_from_string(argv[argind]);
        if (expr != NULL) {
            int value = ast_execute(expr);
            printf("%s = %d\n", argv[argind], value);
            ast_free(expr);
        } else {
            fprintf(stderr, "%s = ERROR: %s\n", argv[argind], strerror(errno));
            status = 1;
        }
    }
    return status;
}
