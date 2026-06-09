#pragma once

#include "mctypes.h"
#include "mctoken.h"
#include "mcheap.h"
#include "mctokenvisitor.h"
#include "mcastvisitor.h"

typedef struct MCExt {
    void** handles;    /* size: max; token handles then AST handles */
    void** visitors;   /* size: max; token visitors then AST visitors */
    u32 tkv_count;
    u32 astv_count;
    u32 max;
} MCExt;

void mcxt_init(MCExt* ext, void* mem, u32 max);
i8 mcxt_load_tkv(MCExt* ext, const char* path);
i8 mcxt_load_astv(MCExt* ext, const char* path);
i8 mcxt_run_tkv(MCExt* ext, TokenArray* arr);
i8 mcxt_run_astv(MCExt* ext, HeapHeader* root);
void mcxt_free(MCExt* ext);
