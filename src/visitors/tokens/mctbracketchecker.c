#include <stdio.h>

#include "mctypes.h"
#include "mctokenvisitor.h"

#include "mctbracketchecker.h"

static i8 mctbc_init(void* ctx, TokenArray* arr) {
	BracketChecker* bc = (BracketChecker*) ctx;

	bc->top = 0;
	bc->token_count = arr->len;

	return 0;
}

static i8 mctbc_visit(void* ctx, Token* tkn, u64 idx) {
	BracketChecker* bc = (BracketChecker*) ctx;
	i64 tk = tkn->token_number;

	if (('(' == tk) || ('[' == tk) || ('{' == tk)) {
		if (bc->top >= (i32) bc->max_depth) {
			printf("bracket: nesting too deep at token %lu\n", idx);
			return -1;
		}
		bc->stack[bc->top] = (i32) idx;
		bc->top += 1;
		bc->partners[idx] = -1;
	} else if ((')' == tk) || (']' == tk) || ('}' == tk)) {
		i32 opener = 0;
		if (0 >= bc->top) {
			printf("bracket: unmatched closing bracket at token %lu\n", idx);
			return -1;
		}
		bc->top -= 1;
		opener = bc->stack[bc->top];
		bc->partners[idx] = opener;
		bc->partners[opener] = (i32) idx;
	} else {
		bc->partners[idx] = -1;
	}

	return 0;
}

static i8 mctbc_deinit(void* ctx) {
	BracketChecker* bc = (BracketChecker*) ctx;

	if (0 != bc->top) {
		printf("bracket: %d unclosed bracket(s)\n", bc->top);
		return -1;
	}

	return 0;
}

void mctbc_setup(TokenVisitor* visitor, BracketChecker* checker,
		i32* partners, u32 max_depth) {
	checker->partners = partners;
	checker->max_depth = max_depth;
	checker->top = 0;

	visitor->init = mctbc_init;
	visitor->visit = mctbc_visit;
	visitor->deinit = mctbc_deinit;
	visitor->ctx = checker;
}
