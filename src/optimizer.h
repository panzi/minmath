#ifndef MINMATH_OPTIMIZER_H__
#define MINMATH_OPTIMIZER_H__
#pragma once

#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AstNode *ast_optimize(struct AstBuffer *buffer, const struct AstNode *expr);

#ifdef __cplusplus
}
#endif

#endif
