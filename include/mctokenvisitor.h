#pragma once

#include "mctypes.h"
#include "mctoken.h"

typedef struct TokenVisitor {
    i8 (*init)  (void* ctx, TokenArray* arr);
    i8 (*visit) (void* ctx, Token* tkn, u64 idx);
    i8 (*deinit)(void* ctx);
    void* ctx;
} TokenVisitor;

typedef TokenVisitor* (*mctkv_load_fn)(void);
