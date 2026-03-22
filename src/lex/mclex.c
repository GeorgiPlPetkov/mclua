#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "mctypes.h"

#include "mclex.h"

static char mclex_peek(LexState* lexstate);
static char mclex_advance(LexState* lexstate);
static char mclex_isid(char c);
static i64 mclex_lexnumber(LexState* lexstate);
static i64 mclex_lexid(LexState* lexstate);
static i64 mclex_set_next_token(LexState* lexstate);
static i64 mclex_lexterminals(LexState* lexstate);

i8 mclex_init(LexState* lexstate) {
    lexstate->token_array.tkns = (Token*) calloc(MAX_TOKENS, sizeof(Token));

    if (NULL == lexstate->token_array.tkns) {
        return -1;
    }

    lexstate->token_array.cap = MAX_TOKENS;
    lexstate->token_array.len = 0;

    return 0;
}

void mclex_free(LexState* lexstate) {
    free(lexstate->token_array.tkns);
}

i8 mclex_lexscript(LexState* lexstate, str8* script) {
    i32 tknnum = 0;

    if ((NULL == lexstate) || (NULL == script)) {
        return -1;
    }

    lexstate->input = script;
    lexstate->curr_char_idx = 0;

    while (lexstate->token_array.len < lexstate->token_array.cap) {
        tknnum = mclex_set_next_token(lexstate);
        if (-1 == tknnum) {
            return -1;
        }

        lexstate->token_array.len += 1;
        if (TK_EOS == tknnum) {
            return 0;
        }
    }

    return -2;
}

void mclex_printtokens(LexState* lstate) {
    TokenArray* tkn_arr = &lstate->token_array;
    char smallstr = 0;
    char* tkn_value = NULL;

    for (u64 idx = 0; idx < tkn_arr->cap; idx += 1) {
        if (FIRST_RESERVED > tkn_arr->tkns[idx].token_number) {
            tkn_value = &smallstr;
            smallstr = tkn_arr->tkns[idx].token_number;
        } else {
            tkn_value = TK2STR(tkn_arr->tkns[idx].token_number);
        }

        printf("[%lu]: %ld | %s | ",
               idx,
               tkn_arr->tkns[idx].token_number,
               tkn_value);

        if (tkn_arr->tkns[idx].token_number == TK_NUMBER) {
            printf("%f", tkn_arr->tkns[idx].semantics.number);
        }
        if (tkn_arr->tkns[idx].token_number == TK_NAME) {
            printf("%s", tkn_arr->tkns[idx].semantics.string.content);
        }

        printf("\n");
    }
}

static i64 mclex_set_next_token(LexState* lexstate) {
    while (isspace(mclex_peek(lexstate))) {
        mclex_advance(lexstate);
    }

    char symbol = mclex_peek(lexstate);
    if (isdigit(symbol)) {
        return mclex_lexnumber(lexstate);
    }
    if (mclex_isid(symbol)) {
        return mclex_lexid(lexstate);
    }

    return mclex_lexterminals(lexstate);
}

static char mclex_peek(LexState* lexstate) {
    if (lexstate->curr_char_idx >= lexstate->input->lenght) {
        return '\0';
    }

    return lexstate->input->content[lexstate->curr_char_idx];
}

static char mclex_advance(LexState* lexstate) {
    if (lexstate->curr_char_idx >= lexstate->input->lenght) {
        return '\0';
    }

    return lexstate->input->content[lexstate->curr_char_idx++];
}

static char mclex_isid(char symbol) {
    return isalnum(symbol) || symbol == '_';
}

static i64 mclex_lexnumber(LexState* lexstate) {
    char numbfr[MAX_NUMLEN];
    i8 idx = 0;

    while (isdigit(mclex_peek(lexstate))) {
        numbfr[idx] = mclex_advance(lexstate);
        idx += 1;

        if (MAX_NUMLEN < (1 + idx)) {
            break;
        }
    }
    numbfr[idx] = '\0';

    CURTKN(lexstate).token_number = TK_NUMBER;
    CURTKN(lexstate).semantics.number = atof(numbfr);
    return TK_NUMBER;
}

static i64 mclex_lexid(LexState* lexstate) {
    char idbfr[MAX_IDLEN];
    i64 idx = 0;

    while (mclex_isid(mclex_peek(lexstate))) {
        idbfr[idx] = mclex_advance(lexstate);
        idx += 1;

        if (MAX_IDLEN < (1 + idx)) {
            break;
        }
    }
    idbfr[idx] = '\0';

    for (idx = FIRST_RESERVED; idx <= TK_WHILE; idx += 1) {
        if (0 == strncmp(idbfr, TK2STR(idx), MAX_IDLEN)) {
            CURTKN(lexstate).token_number = idx;
            return idx;
        }
    }

    // [TODO] hidden allocation is a crime, fix later
    str8_attach(&CURTKN(lexstate).semantics.string,
                strndup(idbfr, MAX_IDLEN));

    CURTKN(lexstate).token_number = TK_NAME;
    return TK_NAME;
}

static i64 mclex_lexterminals(LexState* lexstate) {
    const char termsyms[6] = ".=<>~\0";
    char termbfr[MAX_TERMLEN] = {0};
    i64 idx = 0;
    char symbol = mclex_peek(lexstate);

    while (NULL != strchr(termsyms, symbol)) {
        termbfr[idx] = mclex_advance(lexstate);
        symbol = mclex_peek(lexstate);
        idx += 1;

        if (MAX_TERMLEN <= (1 + idx)) {
            return -1;
        }
    }

    if (1 == idx) {
        symbol = termbfr[0];
    } else if (2 <= idx) {
        termbfr[idx] = '\0';
        for (idx = TK_CONCAT; idx <= TK_EOS; idx += 1) {
            if (0 == strncmp(termbfr, TK2STR(idx), MAX_TERMLEN)) {
                CURTKN(lexstate).token_number = idx;
                return idx;
            }
        }

        return -1;
    }

    mclex_advance(lexstate);
    CURTKN(lexstate).token_number = symbol;
    return symbol;
}
