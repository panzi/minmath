#ifndef MINMATH_TESTDATA_H__
#define MINMATH_TESTDATA_H__
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct TestCase {
    const char *expr;
    char **environ;
    int result;
};

extern const struct TestCase TESTS[];

#ifdef __cplusplus
}
#endif

#endif
