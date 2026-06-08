#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "mclstr.h"

#include "mctypes.h"

#include "mclex.h"

#define WHAT (-2)

static char mclex_peek(LexState* lexstate);
static char mclex_advance(LexState* lexstate);
static char mclex_isid(char c);
static u64 mclex_readoutword(LexState* lexstate, char* scratch, u64 bfrsize);
static u64 mclex_readnumber(LexState* lexstate, char* scratch, u64 bfrsize);
static void mclex_skip_comment(LexState* lexstate);
static i64 mclex_lexnumber(LexState* lexstate);
static i64 mclex_lexid(LexState* lexstate);
static i64 mclex_lexstring(LexState* lexstate, char quote);
static i64 mclex_lexlongstring(LexState* lexstate, u64 level);
static i64 mclex_set_token(LexState* lexstate);
static i64 mclex_lexterminals(LexState* lexstate);
static void mclex_update_file_input(LexState* lexstate);
static i8 mclex_lex(LexState* lexstate);

i8 mclex_init(LexState* lexstate, VMConfig* cfg, byte* lexmem, u64 lexmemcap) {
	u64 scratch_size = 0;
	u64 tokens_size = 0;
	u64 input_size = 0;
	u64 total_size = 0;

	if ((NULL == lexmem) || (NULL == cfg)) {
		return -1;
	}
	lexstate->config = cfg;

	scratch_size = cfg->MAX_IDLEN;
	tokens_size = cfg->MAX_TOKENS * sizeof(Token);
	input_size = cfg->MAX_FILE_CHUNK_SIZE;
	total_size = scratch_size + tokens_size + input_size;

	if (total_size > lexmemcap) {
		printf("lex: %lu total required exceeds %lu pool\n",
				total_size, lexmemcap);
		return -1;
	}

	lexstate->wordscratch_cap = scratch_size;
	lexstate->wordscratch = (char*) lexmem;
	lexmem += scratch_size;

	lexstate->token_array.tkns = (Token*) lexmem;
	lexstate->token_array.cap = cfg->MAX_TOKENS;
	lexstate->token_array.len = 0;
	lexmem += tokens_size;

	lexstate->input = (char*) lexmem;
	lexstate->input_len = 0;

	return 0;
}

void mclex_free(LexState* lexstate) {
	(void) lexstate;
}

i8 mclex_lexscript_str8(LexState* lexstate, str8* script) {
	if ((NULL == lexstate) || (NULL == script)) {
		return -1;
	}

	lexstate->input_file.fhandle = NULL;
	lexstate->input = script->content;
	lexstate->input_len = script->length;
	lexstate->curr_char_idx = 0;

	return mclex_lex(lexstate);
}

i8 mclex_lexscript_file(LexState* lexstate, const char* path) {
	i8 rcode = 0;

	if (0 != mcfs_openfile(&lexstate->input_file, path, "r")) {
		printf("lex: failed to open file %s", path);
		return -1;
	}

	mclex_update_file_input(lexstate);

	if ('#' == mclex_peek(lexstate)) {
		while (('\0' != mclex_peek(lexstate))
				&& ('\n' != mclex_peek(lexstate))) {
			mclex_advance(lexstate);
		}
	}

	rcode = mclex_lex(lexstate);

	fclose(lexstate->input_file.fhandle);
	lexstate->input_file.fhandle = NULL;

	return rcode;
}

void mclex_logtokens(LexState* lstate) {
	TokenArray* tkn_arr = &lstate->token_array;
	char* tkn_value = NULL;
	heap_header* strtkn = NULL;
	i64 tkn_num = 0;
	i64 tkn_idx = 0;
	char chartkn = 0;

	printf("\nTokens:\n");
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
			case TK_INT:
				printf("%ld", tkn_arr->tkns[idx].semantics.integer);
				break;
			case TK_FLT:
				printf("%f", tkn_arr->tkns[idx].semantics.number);
				break;
			case TK_NAME:
				printf("%s", tkn_arr->tkns[idx].semantics.varname);
				break;
			case TK_STRING:
				strtkn = tkn_arr->tkns[idx].semantics.heapobj;
				if (NULL != strtkn) {
					printf("%.*s", (i32) strtkn->object.string->len,
							mclstr_getchars(strtkn));
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
		mclex_advance(lexstate);
	} while ('\0' != symbol);

	if (isdigit((uchar) symbol)) {
		tknnum = mclex_lexnumber(lexstate);
	} else if (mclex_isid(symbol)) {
		tknnum = mclex_lexid(lexstate);
	} else if (('"' == symbol) || ('\'' == symbol)) {
		tknnum = mclex_lexstring(lexstate, symbol);
	} else {
		tknnum = mclex_lexterminals(lexstate);
	}

	TOPTKN(lexstate).token_number = tknnum;
	return tknnum;
}

static char mclex_peek(LexState* lexstate) {
	if (lexstate->curr_char_idx >= lexstate->input_len) {
		if (NULL != lexstate->input_file.fhandle) {
			mclex_update_file_input(lexstate);
		}
		if (lexstate->curr_char_idx >= lexstate->input_len) {
			return '\0';
		}
	}

	return lexstate->input[lexstate->curr_char_idx];
}

static char mclex_advance(LexState* lexstate) {
	if (lexstate->curr_char_idx >= lexstate->input_len) {
		if (NULL != lexstate->input_file.fhandle) {
			mclex_update_file_input(lexstate);
		}

		if (lexstate->curr_char_idx >= lexstate->input_len) {
			return '\0';
		}
	}

	return lexstate->input[lexstate->curr_char_idx++];
}

static char mclex_isid(char symbol) {
	return isalpha((uchar) symbol) || ('_' == symbol);
}

static u64 mclex_readoutword(LexState* lexstate, char* bfr, u64 bfrsize) {
	u64 idx = 0;
	const char termsyms[] = " .=<>~:;,+-*/%^#&|()[]{}\"\'\t\n\r";
	char sym = '\0';

	while ((bfrsize - 1) > idx) {
		sym = mclex_peek(lexstate);
		if (('\0' == sym) || (NULL != strchr(termsyms, sym))) {
			break;
		}

		bfr[idx] = mclex_advance(lexstate);
		idx += 1;
	}
	bfr[idx] = '\0';

	return idx;
}

static u64 mclex_readnumber(LexState* lexstate, char* bfr, u64 bfrsize) {
	u64 idx = 0;
	char sym = '\0';
	char prev = '\0';

	while ((bfrsize - 1) > idx) {
		sym = mclex_peek(lexstate);
		if (isalnum((uchar) sym) || ('.' == sym)) {
			bfr[idx] = mclex_advance(lexstate);
			idx += 1;
			prev = sym;
			continue;
		}

		if ((('+' == sym) || ('-' == sym))
				&& (('e' == prev) || ('E' == prev)
				|| ('p' == prev) || ('P' == prev))) {
			bfr[idx] = mclex_advance(lexstate);
			idx += 1;
			prev = sym;
			continue;
		}
		break;
	}
	bfr[idx] = '\0';

	return idx;
}

static void mclex_skip_comment(LexState* lexstate) {
	char sym = '\0';
	u64 level = 0;
	u64 closing = 0;

	if ('[' == mclex_peek(lexstate)) {
		mclex_advance(lexstate);

		while ('=' == mclex_peek(lexstate)) {
			level += 1;
			mclex_advance(lexstate);
		}

		if ('[' == mclex_peek(lexstate)) {
			mclex_advance(lexstate);

			while ('\0' != (sym = mclex_advance(lexstate))) {
				if (']' != sym) {
					continue;
				}

				closing = 0;
				while ('=' == mclex_peek(lexstate)) {
					closing += 1;
					mclex_advance(lexstate);
				}

				if ((closing == level) && (']' == mclex_peek(lexstate))) {
					mclex_advance(lexstate);
					return;
				}
			}

			return;
		}
	}

	while ('\0' != (sym = mclex_peek(lexstate))) {
		if ('\n' == sym) {
			break;
		}
		mclex_advance(lexstate);
	}
}

static i64 mclex_lexnumber(LexState* lexstate) {
	i64 wordlen = 0;
	u8 is_float = 0;

	wordlen = mclex_readnumber(lexstate,
			lexstate->wordscratch,
			lexstate->wordscratch_cap);

	if (('0' == lexstate->wordscratch[0]) && (wordlen > 2)
			&& (('x' == lexstate->wordscratch[1])
			|| ('X' == lexstate->wordscratch[1]))) {
		TOPTKN(lexstate).semantics.integer = strtol(lexstate->wordscratch, NULL, 16);
		return TK_INT;
	}

	for (i64 i = 0; i < wordlen; i += 1) {
		if (('.' == lexstate->wordscratch[i])
				|| ('e' == lexstate->wordscratch[i])
				|| ('E' == lexstate->wordscratch[i])) {
			is_float = 1;
			break;
		}
	}

	if (is_float) {
		TOPTKN(lexstate).semantics.number = strtod(lexstate->wordscratch, NULL);
		return TK_FLT;
	}

	TOPTKN(lexstate).semantics.integer = strtol(lexstate->wordscratch, NULL, 10);
	return TK_INT;
}

static i64 mclex_lexid(LexState* lexstate) {
	u64 wordlen = mclex_readoutword(lexstate,
			lexstate->wordscratch,
			lexstate->wordscratch_cap);
	char* tkname = NULL;

	for (i64 tokenidx = FIRST_RESERVED; tokenidx <= TK_WHILE; tokenidx += 1) {
		if (0 == strcmp(lexstate->wordscratch, TK2STR(tokenidx))) {
			return tokenidx;
		}
	}

	tkname = mcstrtbl_intern(lexstate->stringtable,
			lexstate->wordscratch,
			wordlen);
	if (NULL == tkname) {
		return TK_ERR;
	}

	TOPTKN(lexstate).semantics.varname = tkname;
	return TK_NAME;
}

static i64 mclex_lexstring(LexState* lexstate, char quote) {
	heap_header* newstr = mclstr_alloc(lexstate->config->MIN_STR_LEN,
			lexstate->heap);
	char* strbfr = lexstate->wordscratch;
	u64 idx = 0;
	char curr = '\0';
	u8 escaped = 0;

	if (NULL == newstr) {
		return TK_ERR;
	}

	mclex_advance(lexstate);
	while (newstr->object.string->len < lexstate->config->MAX_STR_LEN) {
		curr = mclex_peek(lexstate);
		if ('\0' == curr) {
			return TK_ERR;
		}

		if (escaped) {
			escaped = 0;
		} else if ('\\' == curr) {
			escaped = 1;
		} else if (quote == curr) {
			mclex_advance(lexstate);
			break;
		}

		strbfr[idx] = mclex_advance(lexstate);
		idx += 1;

		if (idx >= lexstate->wordscratch_cap) {
			mclstr_append_str0(newstr, strbfr, idx, lexstate->heap);
			idx = 0;
		}
	}

	if (idx > 0) {
		mclstr_append_str0(newstr, strbfr, idx, lexstate->heap);
	}

	TOPTKN(lexstate).semantics.heapobj = newstr;
	return TK_STRING;
}

static i64 mclex_lexlongstring(LexState* lexstate, u64 level) {
	heap_header* newstr = mclstr_alloc(lexstate->config->MIN_STR_LEN,
			lexstate->heap);
	char* strbfr = lexstate->wordscratch;
	u64 idx = 0;
	u64 closing = 0;
	u64 eq = 0;
	char curr = '\0';

	if (NULL == newstr) {
		return TK_ERR;
	}

	if ('\n' == mclex_peek(lexstate)) {
		mclex_advance(lexstate);
	}

	while (newstr->object.string->len < lexstate->config->MAX_STR_LEN) {
		curr = mclex_peek(lexstate);
		if ('\0' == curr) {
			return TK_ERR;
		}

		if (']' == curr) {
			mclex_advance(lexstate);
			closing = 0;
			while ('=' == mclex_peek(lexstate)) {
				closing += 1;
				mclex_advance(lexstate);
			}

			if ((closing == level) && (']' == mclex_peek(lexstate))) {
				mclex_advance(lexstate);
				break;
			}

			strbfr[idx] = ']';
			idx += 1;
			for (eq = 0; eq < closing; eq += 1) {
				strbfr[idx] = '=';
				idx += 1;
			}

			if (idx >= lexstate->wordscratch_cap) {
				mclstr_append_str0(newstr, strbfr, idx, lexstate->heap);
				idx = 0;
			}
			continue;
		}

		strbfr[idx] = mclex_advance(lexstate);
		idx += 1;

		if (idx >= lexstate->wordscratch_cap) {
			mclstr_append_str0(newstr, strbfr, idx, lexstate->heap);
			idx = 0;
		}
	}

	if (idx > 0) {
		mclstr_append_str0(newstr, strbfr, idx, lexstate->heap);
	}

	TOPTKN(lexstate).semantics.heapobj = newstr;
	return TK_STRING;
}

static i64 mclex_lexterminals(LexState* lexstate) {
	u64 len = 0;
	const char simple_temrs[] = "+-*%^#&|;,()[]{}";
	const char complex_temrs[] = ".=<>~/:";
	char symbol = mclex_peek(lexstate);

	if ('\0' == symbol) {
		return TK_EOS;
	}

	if ('-' == symbol) {
		mclex_advance(lexstate);
		if ('-' == mclex_peek(lexstate)) {
			mclex_advance(lexstate);
			mclex_skip_comment(lexstate);
			return WHAT;
		}
		return '-';
	}

	if ('[' == symbol) {
		u64 level = 0;

		mclex_advance(lexstate);
		while ('=' == mclex_peek(lexstate)) {
			level += 1;
			mclex_advance(lexstate);
		}

		if ('[' == mclex_peek(lexstate)) {
			mclex_advance(lexstate);
			return mclex_lexlongstring(lexstate, level);
		}

		if (0 == level) {
			return '[';
		}

		return TK_ERR;
	}

	if (NULL != strchr(simple_temrs, symbol)) {
		mclex_advance(lexstate);
		return symbol;
	}

	if (NULL == strchr(complex_temrs, symbol)) {
		return TK_ERR;
	}

	mclex_advance(lexstate);
	switch (symbol) {
		case '.':
			if ('.' == mclex_peek(lexstate)) {
				mclex_advance(lexstate);
				if ('.' == mclex_peek(lexstate)) {
					mclex_advance(lexstate);
					return TK_DOTS;
				}
				return TK_CONCAT;
			}
			if (isdigit((uchar) mclex_peek(lexstate))) {
				lexstate->wordscratch[0] = '.';
				len = 1 + mclex_readnumber(lexstate,
						lexstate->wordscratch + 1,
						lexstate->wordscratch_cap - 1);
				lexstate->wordscratch[len] = '\0';
				TOPTKN(lexstate).semantics.number =
						strtod(lexstate->wordscratch, NULL);
				return TK_FLT;
			}
			return '.';
		case '=':
			if ('=' == mclex_peek(lexstate)) {
				mclex_advance(lexstate);
				return TK_EQ;
			}
			return '=';
		case '<':
			if ('=' == mclex_peek(lexstate)) {
				mclex_advance(lexstate);
				return TK_LE;
			}
			if ('<' == mclex_peek(lexstate)) {
				mclex_advance(lexstate);
				return TK_SHL;
			}
			return '<';
		case '>':
			if ('=' == mclex_peek(lexstate)) {
				mclex_advance(lexstate);
				return TK_GE;
			}
			if ('>' == mclex_peek(lexstate)) {
				mclex_advance(lexstate);
				return TK_SHR;
			}
			return '>';
		case '~':
			if ('=' == mclex_peek(lexstate)) {
				mclex_advance(lexstate);
				return TK_NE;
			}
			return '~';
		case '/':
			if ('/' == mclex_peek(lexstate)) {
				mclex_advance(lexstate);
				return TK_IDIV;
			}
			return '/';
		case ':':
			if (':' == mclex_peek(lexstate)) {
				mclex_advance(lexstate);
				return TK_DBCOLON;
			}
			return ':';
		default:
			return TK_ERR;
	}
}

static void mclex_update_file_input(LexState* lexstate) {
	lexstate->input_len = mcfs_readchunk(&lexstate->input_file,
			lexstate->input,
			lexstate->config->MAX_FILE_CHUNK_SIZE,
			lexstate->config->MAX_FILE_CHUNK_SIZE);

	lexstate->curr_char_idx = 0;
}

static i8 mclex_lex(LexState* lexstate) {
	i64 tknnum = 0;

	while (lexstate->token_array.len < lexstate->token_array.cap) {
		tknnum = mclex_set_token(lexstate);

		if (TK_ERR == tknnum) {
			return -1;
		}

		if (WHAT == tknnum) {
			continue;
		}

		lexstate->token_array.len += 1;
		if (TK_EOS == tknnum) {
			return 0;
		}
	}

	return -2;
}
