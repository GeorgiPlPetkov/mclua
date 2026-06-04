#pragma once

#include "mctypes.h"
#include "mcheap.h"

typedef struct ASTVisitor {
    i8 (*init)  (void* ctx, heap_header* root);
    i8 (*visit) (void* ctx, heap_header* node, u32 depth);
    i8 (*deinit)(void* ctx);
    void* ctx;
} ASTVisitor;

typedef ASTVisitor* (*mcastv_load_fn)(void);
