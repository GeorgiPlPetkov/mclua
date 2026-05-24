#pragma once

#include "mctoken.h"
#include "mcconfig.h"
#include "mcheap.h"
#include "mcstrtbl.h"

#include "mctypes.h"
#include "mcstr.h"

typedef struct LexState {
    u64 curr_char_idx;
    u64 linecnt;
    u64 nest_depth;
    str8* input;

    TokenArray token_array;
    char* wordscratch;
    u64 wordscratch_cap;

    StringTable* stringtable;
    MCHeap* heap;
    VMConfig* config;
} LexState;

#define TOPTKN(lexstate) \
        ((lexstate)->token_array.tkns[(lexstate)->token_array.len])

i8 mclex_init(LexState* lexstate, byte* lexmem, u64 lexmemcap);
void mclex_free(LexState* lexstate);

i8 mclex_lexscript_str8(LexState* lexstate, str8* script);

void mclex_logtokens(LexState* lexstate);
