#ifndef OPTIMIZER_H__
#define OPTIMIZER_H__
#pragma once

#include "ast.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AstNode *ast_optimize(const struct AstNode *expr);

#ifdef __cplusplus
}
#endif

#endif
