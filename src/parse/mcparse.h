#pragma once

#include "mctoken.h"
#include "mclast.h"

#define CURTKN(pstate) ((pstate)->tokens->tkns[(pstate)->pos])

typedef struct ParseState {
    TokenArray* tokens;
    MCHeap* heap;
    u64 pos;
    u8 error;
    heap_header* root;
} ParseState;

i8 mcparse_init(ParseState* pstate, MCHeap* heap);
i8 mcparse_run(ParseState* pstate, TokenArray* tokens);
void mcparse_free(ParseState* pstate);

void mcparse_log_node(heap_header* node, i32 depth);
