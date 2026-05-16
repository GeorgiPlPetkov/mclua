#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "mctypes.h"

#include "mclex.h"

static char mclex_peek(LexState* lexstate);
static char mclex_advance(LexState* lexstate);
static char mclex_isid(char c);
static u64 mclex_readoutword(LexState* lexstate, char* scratch, u64 ssize);
static f64 mclex_lexnumber(LexState* lexstate);
static i64 mclex_lexid(LexState* lexstate);
static i64 mclex_lexstring(LexState* lexstate);
static i64 mclex_set_token(LexState* lexstate);
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
	for (u32 i = 0; i < lexstate->token_array.len; i += 1) {
		i64 tn = lexstate->token_array.tkns[i].token_number;
		if ((TK_NAME == tn) || (TK_STRING == tn)) {
			str8_free(&lexstate->token_array.tkns[i].semantics.string);
		}
	}
	free(lexstate->token_array.tkns);
}

i8 mclex_lexscript_str8(LexState* lexstate, str8* script) {
	i64 tknnum = 0;

	if ((NULL == lexstate) || (NULL == script)) {
		return -1;
	}

	lexstate->input = script;
	lexstate->curr_char_idx = 0;
	lexstate->linecnt = 0;

	while (lexstate->token_array.len < lexstate->token_array.cap) {
		tknnum = mclex_set_token(lexstate);
		if (TK_ERR == tknnum) {
			return TK_ERR;
		}

		lexstate->token_array.len += 1;
		if (TK_EOS == tknnum) {
			return 0;
		}
	}

	return -2;
}

void mclex_logtokens(LexState* lstate) {
	TokenArray* tkn_arr = &lstate->token_array;
	char  smallstr  = 0;
	char* tkn_value = NULL;
	i64   tkn_num   = 0;
	i64   tkn_idx   = 0;

	for (u64 idx = 0; idx < tkn_arr->len; idx += 1) {
		tkn_num = tkn_arr->tkns[idx].token_number;
		tkn_idx = TK2IDX(tkn_num);

		if (FIRST_RESERVED > tkn_num) {
			tkn_value = &smallstr;
			smallstr  = (char) tkn_num;
		} else if ((tkn_idx >= 0)
		           && ((u64) tkn_idx < (sizeof(tokenstr) / sizeof(tokenstr[0])))) {
			tkn_value = (char*) TK2STR(tkn_num);
		} else {
			tkn_value = "you shouldn't see this";
		}

		printf("[%lu]: %ld | %s | ", idx, tkn_num, tkn_value);

		if (TK_NUMBER == tkn_num) {
			printf("%f", tkn_arr->tkns[idx].semantics.number);
		} else if ((TK_NAME == tkn_num) || (TK_STRING == tkn_num)) {
			printf("%s", tkn_arr->tkns[idx].semantics.string.content);
		}

		printf("\n");
	}
}

static i64 mclex_set_token(LexState* lexstate) {
	char symbol = '\0';
	i64 tknnum = TK_ERR;
	do {
		symbol = mclex_peek(lexstate);
		if (!isspace((uchar) symbol)) {
			break;
		}
		if ('\n' == symbol) {
			lexstate->linecnt += 1;
		}
		mclex_advance(lexstate);
	} while (1);

	if (isdigit((uchar) symbol)) {
		CURTKN(lexstate).token_number = TK_NUMBER;
		CURTKN(lexstate).semantics.number = mclex_lexnumber(lexstate);
		return TK_NUMBER;
	}

	if (mclex_isid(symbol)) {
		// on TK_NAME handles string allocation for name
		tknnum = mclex_lexid(lexstate);
		CURTKN(lexstate).token_number = tknnum;
		return tknnum;
	}

	if ('"' == symbol) {
		// samesies here
		tknnum = mclex_lexstring(lexstate);
		CURTKN(lexstate).token_number = tknnum;
		return tknnum;
	}

	tknnum = mclex_lexterminals(lexstate);
	CURTKN(lexstate).token_number = tknnum;
	return tknnum;
}

static char mclex_peek(LexState* lexstate) {
	if (lexstate->curr_char_idx >= lexstate->input->length) {
		return '\0';
	}

	return lexstate->input->content[lexstate->curr_char_idx];
}

static char mclex_advance(LexState* lexstate) {
	if (lexstate->curr_char_idx >= lexstate->input->length) {
		return '\0';
	}

	return lexstate->input->content[lexstate->curr_char_idx++];
}

static char mclex_isid(char symbol) {
	return isalpha((uchar) symbol) || ('_' == symbol);
}

static u64 mclex_readoutword(LexState* lexstate, char* buf, u64 ssize) {
	u64 idx = 0;
	const char termsyms[] = " .=<>~:;,+-*/%^#&|()[]{}\"\'\t\n\r";
	char sym = '\0';

	while ((ssize - 1) > idx) {
		sym = mclex_peek(lexstate);
		if (('\0' == sym) || (NULL != strchr(termsyms, sym))) {
			break;
		}

		buf[idx] = mclex_advance(lexstate);
		idx += 1;
	}
	buf[idx] = '\0';

	return idx;
}

static f64 mclex_lexnumber(LexState* lexstate) {
	char numbfr[MAX_NUMLEN];
	i64 wordlen = mclex_readoutword(lexstate, numbfr, MAX_NUMLEN);

	if (('0' == numbfr[0]) && (wordlen > 2)
		&& (('x' == numbfr[1]) || ('X' == numbfr[1]))) {
		return (f64) strtol(numbfr + 2, NULL, 16);
	}

	return strtod(numbfr, NULL);
}

static i64 mclex_lexid(LexState* lexstate) {
	char idbfr[MAX_IDLEN];
	u64 wordlen = mclex_readoutword(lexstate, idbfr, MAX_IDLEN);

	for (i64 idx = FIRST_RESERVED; idx <= TK_WHILE; idx += 1) {
		if (0 == strncmp(idbfr, TK2STR(idx), wordlen)) {
			return idx;
		}
	}

	// cool allocation gooes here
	return TK_NAME;
}

static i64 mclex_lexstring(LexState* lexstate) {
	char strbfr[MAX_STRLEN];
	i64 idx = 0;
	char symbol = '\0';

	while ((MAX_STRLEN - 1) > idx) {
		symbol = mclex_peek(lexstate);
		if ('\0' == symbol) {
			return TK_ERR;
		}

		if ('"' == symbol) {
			if ('\\' != strbfr[idx - 1]) {
				mclex_advance(lexstate);
				break;
			}
		}
		strbfr[idx] = mclex_advance(lexstate);
		idx += 1;
	}
	strbfr[idx] = '\0';

	// cool allocation gooes here
	return TK_STRING;
}

static i64 mclex_lexterminals(LexState* lexstate) {
	u64 len = 0;
	const char simple_temrs[] = "+-*/%^#&|:;,()[]{}";
	const char complex_temrs[] = ".=<>~";
	char termbfr[MAX_TERMLEN];
	char symbol = mclex_peek(lexstate);

	if ('\0' == symbol) {
		return TK_EOS;
	}

	if (NULL != strchr(simple_temrs, symbol)) {
		mclex_advance(lexstate);
		return symbol;
	}

	while ((MAX_TERMLEN - 1) > len) {
		symbol = mclex_peek(lexstate);
		if (NULL == strchr(complex_temrs, symbol)) {
			break;
		}
		termbfr[len] = mclex_advance(lexstate);
		len += 1;
	}
	termbfr[len] = '\0';

	if (1 == len) {
		return termbfr[0];
	}

	for (i64 idx = TK_CONCAT; idx <= TK_NE; idx += 1) {
		if (0 == strncmp(termbfr, TK2STR(idx), len)) {
			return idx;
		}
	}

	return TK_ERR;
}
