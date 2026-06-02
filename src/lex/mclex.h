#pragma once

#include "mctoken.h"
#include "mcconfig.h"
#include "mcheap.h"
#include "mcstrtbl.h"

#include "mctypes.h"
#include "mcstr.h"
#include "mcfs.h"

typedef struct LexState {
    u64 curr_char_idx;
    
    mcfile input_file;
    char* input;
    u64 input_len;

    TokenArray token_array;
    char* wordscratch;
    u64 wordscratch_cap;

    StringTable* stringtable;
    MCHeap* heap;
    VMConfig* config;
} LexState;

#define TOPTKN(lexstate) \
        ((lexstate)->token_array.tkns[(lexstate)->token_array.len])

i8 mclex_init(LexState* lexstate, VMConfig* cfg, byte* lexmem, u64 lexmemcap);
void mclex_free(LexState* lexstate);

i8 mclex_lexscript_str8(LexState* lexstate, str8* script);
i8 mclex_lexscript_file(LexState* lexstate, const char* path);

void mclex_logtokens(LexState* lexstate);
