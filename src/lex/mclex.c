#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "mctypes.h"

#include "mclex.h"

#define CURTKN(L) (L->token_array.tkns[L->token_array.len])

#define MAX_TOKENS  (1028)
#define MAX_IDNAMES (32)
#define MAX_NUMLEN  (17)

static char mclex_peek(LexState* lexstate);
static char mclex_advance(LexState* lexstate);
static char mclex_isid(char c);
static void mclex_lexnumber(LexState* lexstate);
static void mclex_lexid(LexState *lexstate);
static void mclex_next_token(LexState* lexstate);

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

i8 mclex_lexall(LexState* lexstate, str8* sstr) {
    if ((NULL == lexstate) || (NULL == sstr)) {
        return -1;
    }

    lexstate->input = sstr;
    lexstate->curr_char_idx = 0;

    while (lexstate->token_array.len < lexstate->token_array.cap) {
        mclex_next_token(lexstate);

        if (CURTKN(lexstate).token_number == TK_EOS) {
            break;
        }

        lexstate->token_array.len += 1;

    }

    return 0;
}

static void mclex_next_token(LexState* lexstate) {
    while (isspace(mclex_peek(lexstate))) {
        mclex_advance(lexstate);
    }

    char c = mclex_peek(lexstate);
    if ('\0' == c) {
        CURTKN(lexstate).token_number = TK_EOS;
    } else if (isdigit(c)) {
        mclex_lexnumber(lexstate);
    } else if (mclex_isid(c)) {
        mclex_lexid(lexstate);
    }

    mclex_advance(lexstate);
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

static void mclex_lexnumber(LexState* lexstate) {
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
}

static void mclex_lexid(LexState *lexstate) {
    char idbfr[MAX_IDNAMES];
    i8 idx = 0;

    while (mclex_isid(mclex_peek(lexstate))) {
        idbfr[idx] = mclex_advance(lexstate);
        idx += 1;

        if (MAX_IDNAMES < (1 + idx)) {
            break;
        }
    }
    idbfr[idx] = '\0';

    i64 tokenid = 0;
    if (0 == strcmp(idbfr, TK2STR(TK_LOCAL))) {
        tokenid = TK_LOCAL;
    } else if (0 == strcmp(idbfr, TK2STR(TK_FUNCTION))) {
        tokenid = TK_FUNCTION;
    } else if (0 == strcmp(idbfr, TK2STR(TK_RETURN))) {
        tokenid = TK_RETURN;
    }
    else {
        tokenid = TK_NAME;
        CURTKN(lexstate).semantics.string.content = strdup(idbfr);
    }

    CURTKN(lexstate).token_number = tokenid;
}
