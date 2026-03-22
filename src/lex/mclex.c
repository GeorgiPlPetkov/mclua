#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "mctypes.h"

#include "mclex.h"

static char mclex_peek(LexState* lexstate);
static char mclex_advance(LexState* lexstate);
static char mclex_isid(char c);
static i64  mclex_lexnumber(LexState* lexstate);
static i64  mclex_lexid(LexState* lexstate);
static i64  mclex_set_next_token(LexState* lexstate);

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

    for (u64 idx = 0; idx < tkn_arr->len; idx += 1) {
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

static i64  mclex_set_next_token(LexState* lexstate) {
    while (isspace(mclex_peek(lexstate))) {
        mclex_advance(lexstate);
    }

    char c = mclex_peek(lexstate);
    if (isdigit(c)) {
        return mclex_lexnumber(lexstate);
    }
    if (mclex_isid(c)) {
        return mclex_lexid(lexstate);
    }

    switch (c) {
        case '\0':
            CURTKN(lexstate).token_number = TK_EOS;
            return TK_EOS;
        case '\r':
            mclex_advance(lexstate);
        case '\n':
            mclex_advance(lexstate);
            lexstate->linecnt += 1;
    }

    mclex_advance(lexstate);
    CURTKN(lexstate).token_number = c;
    return c;
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

static char mclex_isid(char c) {
    return isalnum(c) || c == '_';
}

static i64  mclex_lexnumber(LexState* lexstate) {
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

static i64  mclex_lexid(LexState *lexstate) {
    char idbfr[MAX_NUMLEN];
    i8 idx = 0;

    while (mclex_isid(mclex_peek(lexstate))) {
        idbfr[idx] = mclex_advance(lexstate);
        idx += 1;

        if (MAX_NUMLEN < (1 + idx)) {
            break;
        }
    }
    idbfr[idx] = '\0';

    i32 tokenid = -2;
    if (0 == strcmp(idbfr, TK2STR(TK_LOCAL))) {
        tokenid = TK_LOCAL;
    } else if (0 == strcmp(idbfr, TK2STR(TK_FUNCTION))) {
        tokenid = TK_FUNCTION;
    } else if (0 == strcmp(idbfr, TK2STR(TK_RETURN))) {
        tokenid = TK_RETURN;
    }
    else {
        tokenid = TK_NAME;
        // [TODO] hidden allocation is a crime, fix later
        str8_attach(&CURTKN(lexstate).semantics.string,
                    strndup(idbfr, MAX_NUMLEN));
    }

    CURTKN(lexstate).token_number = tokenid;
    return tokenid;
}
