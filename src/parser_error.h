#ifndef MINMATH_PARSER_ERROR_H__
#define MINMATH_PARSER_ERROR_H__
#pragma once

#include <stddef.h>
#include <stdio.h>

#include "tokenizer.h"

#ifdef __cplusplus
extern "C" {
#endif

enum ParserError {
    PARSER_ERROR_OK = 0,
    PARSER_ERROR_MEMORY,
    PARSER_ERROR_ILLEGAL_TOKEN,
    PARSER_ERROR_EXPECTED_TOKEN,
    PARSER_ERROR_UNEXPECTED_EOF,
};

struct ErrorInfo {
    enum ParserError error;
    size_t offset;
    size_t context_offset;
    enum TokenType token;
};

struct SourceLocation {
    size_t lineno;
    size_t column;
};

struct SourceLocation get_source_location(const char *source, size_t offset);
void print_source_location(FILE *stream, const char *source, size_t offset, size_t context_lines);
void print_parser_error(FILE *stream, const char *source, const struct ErrorInfo *error, size_t context_lines);
void print_error_message(FILE *stream, const struct ErrorInfo *error);
enum ParserError get_error_code(const char *error_name);

#ifdef __cplusplus
}
#endif

#endif
