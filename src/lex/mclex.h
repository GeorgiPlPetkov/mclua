#pragma once

#include <stdio.h>

#include "mctypes.h"
#include "mcstr.h"


#define FIRST_RESERVED (257)
#define TK2IDX(tknum)  (((tknum) - FIRST_RESERVED))

enum RESERVED_TK {
    TK_AND = FIRST_RESERVED, TK_BREAK,
    TK_DO, TK_ELSE, TK_ELSEIF, TK_END, TK_FALSE, TK_FOR, TK_FUNCTION,
    TK_IF, TK_IN, TK_LOCAL, TK_NIL, TK_NOT, TK_OR, TK_REPEAT,
    TK_RETURN, TK_THEN, TK_TRUE, TK_UNTIL, TK_WHILE,
    /* terminal symbols */
    TK_NAME, TK_CONCAT, TK_DOTS, TK_EQ, TK_GE, TK_LE, TK_NE, TK_NUMBER,
    TK_STRING, TK_EOS
};

#define NUM_RESERVED ((i32) (TK_WHILE - FIRST_RESERVE + 1))

/* ORDER RESERVED_TK to arr */
static const char* const tokenstr [] = {
    "and", "break", "do", "else", "elseif",
    "end", "false", "for", "function", "if",
    "in", "local", "nil", "not", "or", "repeat",
    "return", "then", "true", "until", "while", "*name",
    "..", "...", "==", ">=", "<=", "~=",
    "*number", "*string", "<eof>"
};

#define TK2STR(tknum) ((tokenstr[TK2IDX(tknum)]))

typedef union {
    i64  integer;
    f64  number;
    str8 string;
} SemanticInfo;

typedef struct Token {
    i64 token_number;
    SemanticInfo semantics;
} Token;

typedef struct tknarr {
    Token* tkns;
    u32 cap;
    u32 len;
} TokenArray;

typedef struct LexState {
    u64 curr_char_idx;
    u64 linecnt;
    u64 last_consumed_line;
    u64 nest_depth;
    str8* input;
    TokenArray token_array;
} LexState;

i8 mclex_init(LexState* lexstate);
void mclex_free(LexState* lexstate);

i8 mclex_lexall(LexState* lexstate, str8* script);
