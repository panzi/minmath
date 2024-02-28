#include "parser_error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

static size_t find_line_start(const char *source, size_t offset);
static size_t find_line_end(const char *source, size_t offset);
static inline int get_num_len(size_t num);
static void print_source_location_intern(FILE *stream, const char *source, size_t offset, struct SourceLocation loc, size_t context_lines);

enum ParserError get_error_code(const char *error_name) {
    if (strcmp(error_name, "OK") == 0) {
        return PARSER_ERROR_OK;
    }

    if (strcmp(error_name, "MEMORY") == 0) {
        return PARSER_ERROR_MEMORY;
    }

    if (strcmp(error_name, "ILLEGAL_TOKEN") == 0) {
        return PARSER_ERROR_ILLEGAL_TOKEN;
    }

    if (strcmp(error_name, "UNEXPECTED_EOF") == 0) {
        return PARSER_ERROR_UNEXPECTED_EOF;
    }

    return -1;
}

struct SourceLocation get_source_location(const char *source, size_t offset) {
    const char *pos = source + offset;
    const char *ptr = source;
    size_t column = 1;
    size_t lineno = 1;

    while (ptr < pos) {
        if (*ptr == '\n') {
            ++ lineno;
            column = 1;
        } else {
            ++ column;
        }

        ++ ptr;
    }

    return (struct SourceLocation){
        .lineno = lineno,
        .column = column,
    };
}

const char *get_error_message(enum ParserError error) {
    switch (error) {
        case PARSER_ERROR_OK:
            return "Ok";

        case PARSER_ERROR_MEMORY:
            return "Error allocating memory";

        case PARSER_ERROR_ILLEGAL_TOKEN:
            return "Illegal token";

        case PARSER_ERROR_UNEXPECTED_EOF:
            return "Unexpected end of file";

        default:
            assert(false);
            return "Invalid error code";
    }
}

size_t find_line_start(const char *source, size_t offset) {
    const char *ptr = source + offset;
    while (ptr > source && ptr[-1] != '\n') {
        -- ptr;
    }

    return ptr - source;
}

size_t find_line_end(const char *source, size_t offset) {
    const char *ptr = source + offset;
    while (*ptr != '\n' && *ptr) {
        ++ ptr;
    }
    return ptr - source;
}

int get_num_len(size_t num) {
    if (num == 0) {
        return 1;
    }

    int len = 0;
    while (num > 0) {
        ++ len;
        num /= 10;
    }

    return len;
}


void print_source_location(FILE *stream, const char *source, size_t offset, size_t context_lines) {
    struct SourceLocation loc = get_source_location(source, offset);
    print_source_location_intern(stream, source, offset, loc, context_lines);
}

void print_source_location_intern(FILE *stream, const char *source, size_t offset, struct SourceLocation loc, size_t context_lines) {
    const size_t start_lineno = loc.lineno > context_lines ? loc.lineno - context_lines : 1;
    const size_t end_lineno = loc.lineno + context_lines;
    const int lineno_padding = get_num_len(end_lineno);

    fputc('\n', stderr);

    size_t current_lineno = loc.lineno;
    size_t current_offset = find_line_start(source, offset);
    while (current_lineno > start_lineno && current_offset > 0) {
        current_offset = find_line_start(source, current_offset - 1);
        -- current_lineno;
    }

    while (current_lineno <= loc.lineno) {
        size_t next_offset = find_line_end(source, current_offset);
        fprintf(stderr, " %*zu | ", lineno_padding, current_lineno);
        fwrite(source + current_offset, 1, next_offset - current_offset, stderr);
        fputc('\n', stderr);
        ++ current_lineno;
        current_offset = next_offset;
        if (source[current_offset] == '\n') {
            ++ current_offset;
        }
    }

    fprintf(stderr, " %*s   ", lineno_padding, "");
    size_t column = 1;
    while (column < loc.column) {
        fputc('-', stderr);
        ++ column;
    }
    fputc('^', stderr);
    fputc('\n', stderr);

    while (current_lineno <= end_lineno && source[current_offset]) {
        size_t next_offset = find_line_end(source, current_offset);
        fprintf(stderr, " %*zu | ", lineno_padding, current_lineno);
        fwrite(source + current_offset, 1, next_offset - current_offset, stderr);
        fputc('\n', stderr);
        ++ current_lineno;
        current_offset = next_offset;
        if (source[current_offset] == '\n') {
            ++ current_offset;
        }
    }

    fputc('\n', stderr);
}

void print_parser_error(FILE *stream, const char *source, const struct ErrorInfo *error, size_t context_lines) {
    struct SourceLocation loc = get_source_location(source, error->offset);

    fprintf(stderr, "On line %zu at column %zu: %s\n", loc.lineno, loc.column, get_error_message(error->error));

    print_source_location_intern(stream, source, error->offset, loc, context_lines);

    if (error->offset != error->context_offset) {
        fprintf(stderr, "See other location:\n");

        print_source_location(stream, source, error->context_offset, context_lines);
    }
}

#ifdef PARSER_ERROR_BIN
#include <string.h>
#include <errno.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
    if (argc < 4 || argc > 5) {
        fprintf(stderr, "parser_error <ERROR> <OFFSET> [CONTEXT_OFFSET] <SOURCE>\n");
        return 1;
    }

    const char *str_error = argv[1];
    const char *str_offset = argv[2];
    const char *source = argv[argc - 1];

    char *endptr = NULL;
    unsigned long long int offset = strtoull(str_offset, &endptr, 10);
    unsigned long long int context_offset = offset;

    if (!*str_offset || *endptr) {
        fprintf(stderr, "*** error: illegal offset: %s: %s\n", strerror(errno), str_offset);
        return 1;
    }

    if (offset > SIZE_MAX || (size_t) offset > strlen(source) + 1) {
        fprintf(stderr, "*** error: offset %llu overflows source\n", offset);
        return 1;
    }

    if (argc > 4) {
        const char *str_context_offset = argv[3];
        context_offset = strtoull(str_context_offset, &endptr, 10);
        if (!*str_context_offset || *endptr) {
            fprintf(stderr, "*** error: illegal offset: %s: %s\n", strerror(errno), str_context_offset);
            return 1;
        }

        if (context_offset > SIZE_MAX || (size_t) context_offset > strlen(source) + 1) {
            fprintf(stderr, "*** error: context offset %llu overflows source\n", context_offset);
            return 1;
        }
    }

    struct ErrorInfo error = {
        .error = get_error_code(str_error),
        .offset = (size_t) offset,
        .context_offset = (size_t) context_offset,
    };

    print_parser_error(stderr, source, &error, 3);

    return 0;
}
#endif
