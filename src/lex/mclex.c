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

i8 mclex_init(LexState* lexstate, char* lexmem, u64 lexmemcap) {
	u64 scratch_size = 0;
	u64 tokens_size = 0;

	if (NULL == lexmem) {
		return -1;
	}

	scratch_size = lexstate->config->MAX_IDLEN;
	tokens_size = lexstate->config->MAX_TOKENS * sizeof(Token);

	if ((scratch_size + tokens_size) > lexmemcap) {
		printf("lex: %lu tokens + %lu scratch = %lu, exceeds %lu pool\n",
				lexstate->config->MAX_TOKENS, scratch_size,
				scratch_size + tokens_size, lexmemcap);
		return -1;
	}

	lexstate->wordscratch_cap = scratch_size;
	lexstate->wordscratch = lexmem;
	lexmem += scratch_size;

	lexstate->token_array.tkns = (Token*) lexmem;
	lexstate->token_array.cap = lexstate->config->MAX_TOKENS;
	lexstate->token_array.len = 0;

	return 0;
}

void mclex_free(LexState* lexstate) {
	(void) lexstate;
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
	char* tkn_value = NULL;
	str8* strtkn = NULL;
	i64 tkn_num = 0;
	i64 tkn_idx = 0;
	char chartkn = 0;

	printf("tokens:\n");
	for (u64 idx = 0; idx < tkn_arr->len; idx += 1) {
		tkn_num = tkn_arr->tkns[idx].token_number;
		tkn_idx = TK2IDX(tkn_num);

		if (FIRST_RESERVED > tkn_num) {
			tkn_value = &chartkn;
			chartkn = (char) tkn_num;
		} else if ((tkn_idx >= 0)
		           && ((u64) tkn_idx < (sizeof(tokenstr) / sizeof(tokenstr[0])))) {
			tkn_value = (char*) TK2STR(tkn_num);
		} else {
			tkn_value = "you shouldn't see this";
		}

		printf("[%lu]: %ld | %s | ", idx, tkn_num, tkn_value);
		switch (tkn_num) {
			case TK_NUMBER:
				printf("%f", tkn_arr->tkns[idx].semantics.number);
				break;
			case TK_NAME:
				printf("%s", tkn_arr->tkns[idx].semantics.varname);
				break;
			case TK_STRING:
				strtkn = tkn_arr->tkns[idx].semantics.string;
				if (NULL != strtkn) {
					printf("%.*s", (int) strtkn->length, strtkn->content);
				} else {
					printf("null string");
				}
				break;
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
	i64 wordlen = mclex_readoutword(lexstate,
			lexstate->wordscratch,
			lexstate->wordscratch_cap);

	if (('0' == lexstate->wordscratch[0]) && (wordlen > 2)
		&& (('x' == lexstate->wordscratch[1])
			|| ('X' == lexstate->wordscratch[1]))) {
		return (f64) strtol(lexstate->wordscratch + 2, NULL, 16);
	}

	return strtod(lexstate->wordscratch, NULL);
}

static i64 mclex_lexid(LexState* lexstate) {
	u64 wordlen = mclex_readoutword(lexstate,
			lexstate->wordscratch,
			lexstate->wordscratch_cap);
	char* tkname = NULL;

	for (i64 tokenidx = FIRST_RESERVED; tokenidx <= TK_WHILE; tokenidx += 1) {
		if (0 == strncmp(lexstate->wordscratch, TK2STR(tokenidx), wordlen)) {
			return tokenidx;
		}
	}

	tkname = mcstrtbl_intern(lexstate->stringtable,
			lexstate->wordscratch,
			wordlen);
	if (NULL == tkname) {
		return TK_ERR;
	}

	CURTKN(lexstate).semantics.varname = tkname;
	return TK_NAME;
}

static i64 mclex_lexstring(LexState* lexstate) {
	char strbfr[67];
	i64 idx = 0;
	char symbol = '\0';

	while ((67 - 1) > idx) {
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
	char symbol = mclex_peek(lexstate);

	if ('\0' == symbol) {
		return TK_EOS;
	}

	if (NULL != strchr(simple_temrs, symbol)) {
		mclex_advance(lexstate);
		return symbol;
	}

	while ((lexstate->wordscratch_cap - 1) > len) {
		symbol = mclex_peek(lexstate);
		if (NULL == strchr(complex_temrs, symbol)) {
			break;
		}
		lexstate->wordscratch[len] = mclex_advance(lexstate);
		len += 1;
	}
	lexstate->wordscratch[len] = '\0';

	if (1 == len) {
		return lexstate->wordscratch[0];
	}

	for (i64 idx = TK_CONCAT; idx <= TK_NE; idx += 1) {
		if (0 == strncmp(lexstate->wordscratch, TK2STR(idx), len)) {
			return idx;
		}
	}

	return TK_ERR;
}
