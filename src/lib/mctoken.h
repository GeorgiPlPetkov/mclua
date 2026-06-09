#pragma once

#include "mctypes.h"
#include "mcheap.h"

#define FIRST_RESERVED (257)

enum RESERVED_TK {
    TK_AND = FIRST_RESERVED, TK_BREAK,
    TK_DO, TK_ELSE, TK_ELSEIF, TK_END, TK_FALSE, TK_FOR, TK_FUNCTION,
    TK_GOTO, TK_IF, TK_IN, TK_LOCAL, TK_NIL, TK_NOT, TK_OR, TK_REPEAT,
    TK_RETURN, TK_THEN, TK_TRUE, TK_UNTIL, TK_WHILE, TK_NAME,
    /* long terminal symbols */
    TK_CONCAT, TK_DOTS, TK_EQ, TK_GE, TK_LE, TK_NE,
    TK_IDIV, TK_SHL, TK_SHR, TK_DBCOLON,
    TK_INT, TK_FLT, TK_STRING, TK_EOS,
    TK_ERR = 8, // backspace
};

/* ORDER RESERVED_TK to arr */
static char* tokenstr[] = {
    "and", "break", "do", "else", "elseif", "end", "false", "for", "function",
    "goto", "if", "in", "local", "nil", "not", "or", "repeat",
    "return", "then", "true", "until", "while", "*name",
    /* long terminal symbols */
    "..", "...", "==", ">=", "<=", "~=",
    "//", "<<", ">>", "::",
    "*int", "*float", "*string", "<eos>",
    "<lexerr>"
};

typedef union SemanticInfo {
    i64 integer;
    f64 number;
    char* varname;
    HeapHeader* heapobj;
} SemanticInfo;

typedef struct Token {
    i64 token_number;
    SemanticInfo semantics;
} Token;

typedef struct TokenArray {
    Token* tkns;
    u32 cap;
    u32 len;
} TokenArray;

#define TOKEN_SIZE (sizeof(Token))

#define NUM_RESERVED ((i32) (TK_WHILE - FIRST_RESERVED + 1))

#define TK2IDX(token_num) ((token_num) - (FIRST_RESERVED))
#define TK2STR(token_num) (tokenstr[TK2IDX(token_num)])
