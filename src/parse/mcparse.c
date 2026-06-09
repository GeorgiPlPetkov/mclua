#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mclstr.h"

#include "mcparse.h"

static HeapHeader* parse_block(ParseState* pstate);
static HeapHeader* parse_stat(ParseState* pstate);
static HeapHeader* parse_retstat(ParseState* pstate);
static HeapHeader* parse_exp(ParseState* pstate);
static HeapHeader* parse_subexp(ParseState* pstate, u8 min_prec);
static HeapHeader* parse_simpleexp(ParseState* pstate);
static HeapHeader* parse_suffixedexp(ParseState* pstate);
static HeapHeader* parse_primaryexp(ParseState* pstate);
static HeapHeader* parse_tableconstructor(ParseState* pstate);
static HeapHeader* parse_funcbody(ParseState* pstate);
static HeapHeader* parse_function(ParseState* pstate, u8 is_main);
static i8 parse_funcname_into(ParseState* pstate, HeapHeader* name);
static i8 parse_arglist(ParseState* pstate, HeapHeader* arglist);
static HeapHeader* parse_explist(ParseState* pstate);
static HeapHeader* parse_args(ParseState* pstate);
static HeapHeader* parse_attname(ParseState* pstate);
static HeapHeader* mcparse_parseroot(ParseState* pstate, TokenArray* tokens);

static i8 drain_children(ParseState* pstate, HeapHeader* dst, HeapHeader* src) {
	u32 count = AST_NCHILD(src);
	for (u32 cidx = 0; cidx < count; cidx += 1) {
		if (0 != mclast_push_child(dst, AST_CHILD(src, cidx), pstate->heap)) {
			return -1;
		}
	}
	return 0;
}

static Token* ps_advance(ParseState* pstate) {
	Token* t = &CURTKN(pstate);
	if (pstate->pos < pstate->tokens->len) {
		pstate->pos++;
	}
	return t;
}

static i8 ps_match(ParseState* pstate, i64 tk) {
	if (tk == CURTKN(pstate).token_number) {
		ps_advance(pstate);
		return 1;
	}
	return 0;
}

static Token* ps_expect(ParseState* pstate, i64 tk) {
	if (tk != CURTKN(pstate).token_number) {
		if (tk < FIRST_RESERVED) {
			printf("parse error: expected '%c' got token %ld at pos %lu\n",
					(char) tk, CURTKN(pstate).token_number, pstate->pos);
		} else {
			printf("parse error: expected %s got token %ld at pos %lu\n",
					TK2STR(tk), CURTKN(pstate).token_number, pstate->pos);
		}
		pstate->error = 1;
		return NULL;
	}
	return ps_advance(pstate);
}

static i8 is_block_end(i64 tk) {
	return (TK_END == tk) || (TK_ELSE == tk) || (TK_ELSEIF == tk)
			|| (TK_UNTIL == tk) || (TK_EOS == tk);
}

typedef struct {
	u8 left;
	u8 right;
} BinOpPrio;
static const BinOpPrio binop_prio_tbl[TK_EOS + 1] = {
	[TK_OR] = {1, 1},
	[TK_AND] = {2, 2},
	['<'] = {3, 3},
	['>'] = {3, 3},
	[TK_LE] = {3, 3},
	[TK_GE] = {3, 3},
	[TK_EQ] = {3, 3},
	[TK_NE] = {3, 3},
	['|'] = {4, 4},
	['~'] = {5, 5},
	['&'] = {6, 6},
	[TK_SHL] = {7, 7},
	[TK_SHR] = {7, 7},
	[TK_CONCAT] = {8, 7},
	['+'] = {9, 9},
	['-'] = {9, 9},
	['*'] = {10, 10},
	['/'] = {10, 10},
	[TK_IDIV] = {10, 10},
	['%'] = {10, 10},
	['^'] = {12, 11},
};

#define UNOP_PRIO 11
static i8 is_unop(i64 tk) {
	return ('-' == tk) || (TK_NOT == tk) || ('#' == tk) || ('~' == tk);
}

static HeapHeader* parse_primaryexp(ParseState* pstate) {
	HeapHeader* node = NULL;
	Token* tkn = NULL;

	if (TK_NAME == CURTKN(pstate).token_number) {
		tkn = ps_advance(pstate);
		node = mclast_alloc(PN_NAME, 0, pstate->heap);
		if (NULL == node) {
			return NULL;
		}

		AST_VAL(node).varname = tkn->semantics.varname;
		return node;
	}

	if ('(' == CURTKN(pstate).token_number) {
		HeapHeader* paren = NULL;
		i64 inner = 0;

		ps_advance(pstate);
		node = parse_exp(pstate);
		if (NULL == node) {
			return NULL;
		}
		if (NULL == ps_expect(pstate, ')')) {
			return NULL;
		}

		inner = AST_TYPE(node);
		if ((PN_CALL == inner) || (PN_CALL_METHOD == inner)
				|| (PN_VARARG == inner)) {
			paren = mclast_alloc(PN_PAREN, 1, pstate->heap);
			if (NULL == paren) {
				return NULL;
			}
			if (0 != mclast_push_child(paren, node, pstate->heap)) {
				return NULL;
			}
			return paren;
		}
		return node;
	}

	printf("parse error: unexpected token %ld in primary exp at pos %lu\n",
			CURTKN(pstate).token_number, pstate->pos);
	pstate->error = 1;
	return NULL;
}

static HeapHeader* parse_args(ParseState* pstate) {
	HeapHeader* args = NULL;
	HeapHeader* elist = NULL;
	HeapHeader* tbl = NULL;
	HeapHeader* strnode = NULL;
	Token* tk = NULL;

	args = mclast_alloc(PN_EXPLIST, 4, pstate->heap);
	if (NULL == args) {
		return NULL;
	}

	if ('(' == CURTKN(pstate).token_number) {
		ps_advance(pstate);
		if (')' != CURTKN(pstate).token_number) {
			elist = parse_explist(pstate);
			if (NULL == elist) {
				return NULL;
			}
			if (0 != drain_children(pstate, args, elist)) {
				return NULL;
			}
		}
		if (NULL == ps_expect(pstate, ')')) {
			return NULL;
		}
		return args;
	}

	if ('{' == CURTKN(pstate).token_number) {
		tbl = parse_tableconstructor(pstate);
		if (NULL == tbl) {
			return NULL;
		}
		if (0 != mclast_push_child(args, tbl, pstate->heap)) {
			return NULL;
		}
		return args;
	}

	if (TK_STRING == CURTKN(pstate).token_number) {
		tk = ps_advance(pstate);
		strnode = mclast_alloc(PN_STRING, 0, pstate->heap);
		if (NULL == strnode) {
			return NULL;
		}
		AST_VAL(strnode).heapobj = tk->semantics.heapobj;
		if (0 != mclast_push_child(args, strnode, pstate->heap)) {
			return NULL;
		}
		return args;
	}

	printf("parse error: expected function args at pos %lu\n", pstate->pos);
	pstate->error = 1;
	return NULL;
}

static HeapHeader* parse_suffixedexp(ParseState* pstate) {
	HeapHeader* expr = NULL;
	HeapHeader* field = NULL;
	HeapHeader* key = NULL;
	HeapHeader* idx = NULL;
	HeapHeader* raw_args = NULL;
	HeapHeader* call = NULL;
	Token* name_tok = NULL;

	expr = parse_primaryexp(pstate);
	if (NULL == expr) {
		return NULL;
	}

	while (TK_EOS != CURTKN(pstate).token_number) {
		switch (CURTKN(pstate).token_number) {
			case '.':
				ps_advance(pstate);
				name_tok = ps_expect(pstate, TK_NAME);
				if (NULL == name_tok) {
					return NULL;
				}

				field = mclast_alloc(PN_FIELD, 1, pstate->heap);
				if (NULL == field) {
					return NULL;
				}

				AST_VAL(field).varname = name_tok->semantics.varname;
				mclast_push_child(field, expr, pstate->heap);
				expr = field;
				break;

			case '[':
				ps_advance(pstate);
				key = parse_exp(pstate);
				if (NULL == key) {
					return NULL;
				}

				if (NULL == ps_expect(pstate, ']')) {
					return NULL;
				}

				idx = mclast_alloc(PN_INDEX, 2, pstate->heap);
				if (NULL == idx) {
					return NULL;
				}

				mclast_push_child(idx, expr, pstate->heap);
				mclast_push_child(idx, key, pstate->heap);
				expr = idx;
				break;

			case ':':
				ps_advance(pstate);
				name_tok = ps_expect(pstate, TK_NAME);
				if (NULL == name_tok) {
					return NULL;
				}

				raw_args = parse_args(pstate);
				if (NULL == raw_args) {
					return NULL;
				}

				call = mclast_alloc(PN_CALL_METHOD, 2, pstate->heap);
				if (NULL == call) {
					return NULL;
				}

				AST_VAL(call).varname = name_tok->semantics.varname;
				if (0 != mclast_push_child(call, expr, pstate->heap)) {
					return NULL;
				}

				if (0 != drain_children(pstate, call, raw_args)) {
					return NULL;
				}
				expr = call;
				break;

			case '(':
			case '{':
			case TK_STRING:
				raw_args = parse_args(pstate);
				if (NULL == raw_args) {
					return NULL;
				}

				call = mclast_alloc(PN_CALL, 2, pstate->heap);
				if (NULL == call) {
					return NULL;
				}

				if (0 != mclast_push_child(call, expr, pstate->heap)) {
					return NULL;
				}

				if (0 != drain_children(pstate, call, raw_args)) {
					return NULL;
				}
				expr = call;
				break;

			default:
				return expr;
		}
	}

	return expr;
}

static HeapHeader* parse_simpleexp(ParseState* pstate) {
	HeapHeader* node = NULL;
	Token* tk = NULL;

	if ('{' == CURTKN(pstate).token_number) {
		return parse_tableconstructor(pstate);
	}

	switch (CURTKN(pstate).token_number) {
		case TK_INT:
			tk = ps_advance(pstate);
			node = mclast_alloc(PN_INTEGER, 0, pstate->heap);
			if (NULL == node) {
				return NULL;
			}

			AST_VAL(node).integer = tk->semantics.integer;
			return node;
		case TK_FLT:
			tk = ps_advance(pstate);
			node = mclast_alloc(PN_FLOAT, 0, pstate->heap);
			if (NULL == node) {
				return NULL;
			}

			AST_VAL(node).number = tk->semantics.number;
			return node;
		case TK_STRING:
			tk = ps_advance(pstate);
			node = mclast_alloc(PN_STRING, 0, pstate->heap);
			if (NULL == node) {
				return NULL;
			}

			AST_VAL(node).heapobj = tk->semantics.heapobj;
			return node;
		case TK_NIL:
			ps_advance(pstate);
			return mclast_alloc(PN_NIL, 0, pstate->heap);
		case TK_TRUE:
			ps_advance(pstate);
			return mclast_alloc(PN_TRUE, 0, pstate->heap);
		case TK_FALSE:
			ps_advance(pstate);
			return mclast_alloc(PN_FALSE, 0, pstate->heap);
		case TK_DOTS:
			ps_advance(pstate);
			return mclast_alloc(PN_VARARG, 0, pstate->heap);
		case TK_FUNCTION: {
			HeapHeader* fexpr = NULL;
			HeapHeader* body = NULL;

			ps_advance(pstate);
			body = parse_funcbody(pstate);
			if (NULL == body) {
				return NULL;
			}

			fexpr = mclast_alloc(PN_FUNC_EXPR, 1, pstate->heap);
			if (NULL == fexpr) {
				return NULL;
			}

			mclast_push_child(fexpr, body, pstate->heap);
			return fexpr;
		}
		default:
			return parse_suffixedexp(pstate);
	}
}

static HeapHeader* parse_subexp(ParseState* pstate, u8 min_prec) {
	HeapHeader* lefthand_expr = NULL;
	HeapHeader* righthand_expr = NULL;
	HeapHeader* operand = NULL;
	HeapHeader* binop = NULL;
	BinOpPrio prio = {0, 0};
	i64 tknum = CURTKN(pstate).token_number;

	if (is_unop(tknum)) {
		ps_advance(pstate);
		operand = parse_subexp(pstate, UNOP_PRIO);
		if (NULL == operand) {
			return NULL;
		}

		lefthand_expr = mclast_alloc(PN_UNOP, 1, pstate->heap);
		if (NULL == lefthand_expr) {
			return NULL;
		}

		AST_VAL(lefthand_expr).integer = tknum;
		mclast_push_child(lefthand_expr, operand, pstate->heap);
	} else {
		lefthand_expr = parse_simpleexp(pstate);
		if (NULL == lefthand_expr) {
			return NULL;
		}
	}

	tknum = CURTKN(pstate).token_number;
	prio = binop_prio_tbl[tknum];
	while (prio.left > min_prec) {
		ps_advance(pstate);
		righthand_expr = parse_subexp(pstate, prio.right);
		if (NULL == righthand_expr) {
			return NULL;
		}

		binop = mclast_alloc(PN_BINOP, 2, pstate->heap);
		if (NULL == binop) {
			return NULL;
		}

		AST_VAL(binop).integer = tknum;
		mclast_push_child(binop, lefthand_expr, pstate->heap);
		mclast_push_child(binop, righthand_expr, pstate->heap);
		lefthand_expr = binop;
		tknum = CURTKN(pstate).token_number;
		prio = binop_prio_tbl[tknum];
	}

	return lefthand_expr;
}

static HeapHeader* parse_exp(ParseState* pstate) {
	return parse_subexp(pstate, 0);
}

static HeapHeader* parse_explist(ParseState* pstate) {
	HeapHeader* list = NULL;
	HeapHeader* exp = NULL;

	list = mclast_alloc(PN_EXPLIST, 4, pstate->heap);
	if (NULL == list) {
		return NULL;
	}

	exp = parse_exp(pstate);
	if (NULL == exp) {
		return NULL;
	}

	if (0 != mclast_push_child(list, exp, pstate->heap)) {
		return NULL;
	}

	while (ps_match(pstate, ',')) {
		exp = parse_exp(pstate);
		if (NULL == exp) {
			return NULL;
		}
		if (0 != mclast_push_child(list, exp, pstate->heap)) {
			return NULL;
		}
	}

	return list;
}

static HeapHeader* parse_tableconstructor(ParseState* pstate) {
	HeapHeader* tbl = NULL;
	HeapHeader* field = NULL;
	HeapHeader* key = NULL;
	HeapHeader* val = NULL;
	Token* name_tok = NULL;
	u64 saved_pos = 0;

	tbl = mclast_alloc(PN_TABLE, 4, pstate->heap);
	if (NULL == tbl) {
		return NULL;
	}

	if (NULL == ps_expect(pstate, '{')) {
		return NULL;
	}

	while ('}' != CURTKN(pstate).token_number && TK_EOS != CURTKN(pstate).token_number) {
		field = NULL;

		if ('[' == CURTKN(pstate).token_number) {
			ps_advance(pstate);
			key = parse_exp(pstate);
			if (NULL == key) {
				return NULL;
			}

			if (NULL == ps_expect(pstate, ']')) {
				return NULL;
			}

			if (NULL == ps_expect(pstate, '=')) {
				return NULL;
			}

			val = parse_exp(pstate);
			if (NULL == val) {
				return NULL;
			}

			field = mclast_alloc(PN_FIELD_IDX, 2, pstate->heap);
			if (NULL == field) {
				return NULL;
			}

			mclast_push_child(field, key, pstate->heap);
			mclast_push_child(field, val, pstate->heap);
		} else if (TK_NAME == CURTKN(pstate).token_number) {
			saved_pos = pstate->pos;
			name_tok = ps_advance(pstate);

			if ('=' == CURTKN(pstate).token_number) {
				ps_advance(pstate);
				val = parse_exp(pstate);
				if (NULL == val) {
					return NULL;
				}

				field = mclast_alloc(PN_FIELD_NAME, 1, pstate->heap);
				if (NULL == field) {
					return NULL;
				}

				AST_VAL(field).varname = name_tok->semantics.varname;
				mclast_push_child(field, val, pstate->heap);
			} else {
				pstate->pos = saved_pos;
				val = parse_exp(pstate);
				if (NULL == val) {
					return NULL;
				}

				field = mclast_alloc(PN_FIELD_VAL, 1, pstate->heap);
				if (NULL == field) {
					return NULL;
				}
				mclast_push_child(field, val, pstate->heap);
			}
		} else {
			val = parse_exp(pstate);
			if (NULL == val) {
				return NULL;
			}

			field = mclast_alloc(PN_FIELD_VAL, 1, pstate->heap);
			if (NULL == field) {
				return NULL;
			}
			mclast_push_child(field, val, pstate->heap);
		}

		if (0 != mclast_push_child(tbl, field, pstate->heap)) {
			return NULL;
		}

		if (!ps_match(pstate, ',') && !ps_match(pstate, ';')) {
			break;
		}
	}

	if (NULL == ps_expect(pstate, '}')) {
		return NULL;
	}

	return tbl;
}

static i8 parse_arglist(ParseState* pstate, HeapHeader* arglist) {
	Token* name_tok = NULL;
	HeapHeader* currname = NULL;

	if (')' == CURTKN(pstate).token_number) {
		goto CLEANARGLIST;
	}

	if (TK_DOTS == CURTKN(pstate).token_number) {
		ps_advance(pstate);
		AST_VAL(arglist).integer = 1;
		goto CLEANARGLIST;
	}

	while (')' != CURTKN(pstate).token_number) {
		name_tok = ps_expect(pstate, TK_NAME);
		if (NULL == name_tok) {
			return -1;
		}

		currname = mclast_alloc(PN_NAME, 0, pstate->heap);
		if (NULL == currname) {
			return -1;
		}

		AST_VAL(currname).varname = name_tok->semantics.varname;
		if (0 != mclast_push_child(arglist, currname, pstate->heap)) {
			return -1;
		}

		if (!ps_match(pstate, ',')) {
			break;
		}

		if (TK_DOTS == CURTKN(pstate).token_number) {
			ps_advance(pstate);
			AST_VAL(arglist).integer = 1;
			break;
		}
	}

CLEANARGLIST:
	return 0;
}

static HeapHeader* parse_funcbody(ParseState* pstate) {
	HeapHeader* body = NULL;
	HeapHeader* pl = NULL;
	HeapHeader* blk = NULL;

	body = mclast_alloc(PN_FUNCBODY, 2, pstate->heap);
	if (NULL == body) {
		return NULL;
	}

	if (NULL == ps_expect(pstate, '(')) {
		return NULL;
	}

	pl = mclast_alloc(PN_ARGLIST, 4, pstate->heap);
	if (NULL == pl) {
		return NULL;
	}

	if (0 != parse_arglist(pstate, pl)) {
		return NULL;
	}

	mclast_push_child(body, pl, pstate->heap);

	if (NULL == ps_expect(pstate, ')')) {
		return NULL;
	}

	blk = parse_block(pstate);
	if (NULL == blk) {
		return NULL;
	}

	mclast_push_child(body, blk, pstate->heap);

	if (NULL == ps_expect(pstate, TK_END)) {
		return NULL;
	}
	return body;
}

static i8 parse_funcname_into(ParseState* pstate, HeapHeader* name) {
	Token* tok = NULL;
	HeapHeader* part = NULL;

	tok = ps_expect(pstate, TK_NAME);
	if (NULL == tok) {
		return -1;
	}

	AST_VAL(name).varname = tok->semantics.varname;
	while ('.' == CURTKN(pstate).token_number) {
		ps_advance(pstate);
		tok = ps_expect(pstate, TK_NAME);
		if (NULL == tok) {
			return -1;
		}
		part = mclast_alloc(PN_NAME, 0, pstate->heap);
		if (NULL == part) {
			return -1;
		}
		AST_VAL(part).varname = tok->semantics.varname;
		if (0 != mclast_push_child(name, part, pstate->heap)) {
			return -1;
		}
	}

	if (':' == CURTKN(pstate).token_number) {
		ps_advance(pstate);
		tok = ps_expect(pstate, TK_NAME);
		if (NULL == tok) {
			return -1;
		}

		part = mclast_alloc(PN_NAME, 0, pstate->heap);
		if (NULL == part) {
			return -1;
		}

		AST_VAL(part).varname = tok->semantics.varname;
		if (0 != mclast_push_child(name, part, pstate->heap)) {
			return -1;
		}

		return 1;
	}

	return 0;
}

static HeapHeader* parse_attname(ParseState* pstate) {
	Token* name_tok = NULL;
	Token* attr_tok = NULL;
	HeapHeader* an = NULL;
	HeapHeader* attr = NULL;

	name_tok = ps_expect(pstate, TK_NAME);
	if (NULL == name_tok) {
		return NULL;
	}

	an = mclast_alloc(PN_ATTNAME, 1, pstate->heap);
	if (NULL == an) {
		return NULL;
	}

	AST_VAL(an).varname = name_tok->semantics.varname;
	if ('<' == CURTKN(pstate).token_number) {
		ps_advance(pstate);
		attr_tok = ps_expect(pstate, TK_NAME);
		if (NULL == attr_tok) {
			return NULL;
		}

		attr = mclast_alloc(PN_NAME, 0, pstate->heap);
		if (NULL == attr) {
			return NULL;
		}

		AST_VAL(attr).varname = attr_tok->semantics.varname;
		mclast_push_child(an, attr, pstate->heap);
		if (NULL == ps_expect(pstate, '>')) {
			return NULL;
		}
	}

	return an;
}

static HeapHeader* parse_retstat(ParseState* pstate) {
	HeapHeader* ret = NULL;
	HeapHeader* el = NULL;

	ret = mclast_alloc(PN_RETURN, 4, pstate->heap);
	if (NULL == ret) {
		return NULL;
	}

	ps_advance(pstate);

	if (!is_block_end(CURTKN(pstate).token_number)
			&& (';' != CURTKN(pstate).token_number)) {
		el = parse_explist(pstate);
		if (NULL == el) {
			return NULL;
		}
		if (0 != drain_children(pstate, ret, el)) {
			return NULL;
		}
	}

	ps_match(pstate, ';');
	return ret;
}

static HeapHeader* parse_if(ParseState* pstate) {
	HeapHeader* stat = NULL;
	HeapHeader* cond = NULL;
	HeapHeader* blk = NULL;
	HeapHeader* ei_cond = NULL;
	HeapHeader* ei_blk = NULL;
	HeapHeader* elseif = NULL;
	HeapHeader* else_blk = NULL;
	HeapHeader* else_node = NULL;

	ps_advance(pstate);

	cond = parse_exp(pstate);
	if (NULL == cond) {
		return NULL;
	}

	if (NULL == ps_expect(pstate, TK_THEN)) {
		return NULL;
	}

	blk = parse_block(pstate);
	if (NULL == blk) {
		return NULL;
	}

	stat = mclast_alloc(PN_IF, 4, pstate->heap);
	if (NULL == stat) {
		return NULL;
	}

	if (0 != mclast_push_child(stat, cond, pstate->heap)) {
		return NULL;
	}

	if (0 != mclast_push_child(stat, blk, pstate->heap)) {
		return NULL;
	}

	while (TK_ELSEIF == CURTKN(pstate).token_number) {
		ps_advance(pstate);
		ei_cond = parse_exp(pstate);
		if (NULL == ei_cond) {
			return NULL;
		}

		if (NULL == ps_expect(pstate, TK_THEN)) {
			return NULL;
		}

		ei_blk = parse_block(pstate);
		if (NULL == ei_blk) {
			return NULL;
		}

		elseif = mclast_alloc(PN_ELSEIF, 2, pstate->heap);
		if (NULL == elseif) {
			return NULL;
		}

		mclast_push_child(elseif, ei_cond, pstate->heap);
		mclast_push_child(elseif, ei_blk, pstate->heap);

		if (0 != mclast_push_child(stat, elseif, pstate->heap)) {
			return NULL;
		}
	}

	if (TK_ELSE == CURTKN(pstate).token_number) {
		ps_advance(pstate);
		else_blk = parse_block(pstate);
		if (NULL == else_blk) {
			return NULL;
		}

		else_node = mclast_alloc(PN_ELSE, 1, pstate->heap);
		if (NULL == else_node) {
			return NULL;
		}

		mclast_push_child(else_node, else_blk, pstate->heap);

		if (0 != mclast_push_child(stat, else_node, pstate->heap)) {
			return NULL;
		}
	}

	if (NULL == ps_expect(pstate, TK_END)) {
		return NULL;
	}
	return stat;
}

static HeapHeader* parse_while(ParseState* pstate) {
	HeapHeader* stat = NULL;
	HeapHeader* cond = NULL;
	HeapHeader* blk = NULL;

	ps_advance(pstate);

	cond = parse_exp(pstate);
	if (NULL == cond) {
		return NULL;
	}

	if (NULL == ps_expect(pstate, TK_DO)) {
		return NULL;
	}

	blk = parse_block(pstate);
	if (NULL == blk) {
		return NULL;
	}

	if (NULL == ps_expect(pstate, TK_END)) {
		return NULL;
	}

	stat = mclast_alloc(PN_WHILE, 2, pstate->heap);
	if (NULL == stat) {
		return NULL;
	}

	mclast_push_child(stat, cond, pstate->heap);
	mclast_push_child(stat, blk, pstate->heap);

	return stat;
}

static HeapHeader* parse_do(ParseState* pstate) {
	HeapHeader* stat = NULL;
	HeapHeader* blk = NULL;

	ps_advance(pstate);

	blk = parse_block(pstate);
	if (NULL == blk) {
		return NULL;
	}

	if (NULL == ps_expect(pstate, TK_END)) {
		return NULL;
	}

	stat = mclast_alloc(PN_DO, 1, pstate->heap);
	if (NULL == stat) {
		return NULL;
	}

	mclast_push_child(stat, blk, pstate->heap);
	return stat;
}

static HeapHeader* parse_for(ParseState* pstate) {
	HeapHeader* stat = NULL;
	HeapHeader* start = NULL;
	HeapHeader* limit = NULL;
	HeapHeader* step = NULL;
	HeapHeader* blk = NULL;
	HeapHeader* nm = NULL;
	HeapHeader* el = NULL;
	Token* first_name = NULL;
	Token* n = NULL;

	ps_advance(pstate);

	first_name = ps_expect(pstate, TK_NAME);
	if (NULL == first_name) {
		return NULL;
	}

	if ('=' == CURTKN(pstate).token_number) {
		ps_advance(pstate);
		start = parse_exp(pstate);
		if (NULL == start) {
			return NULL;
		}

		if (NULL == ps_expect(pstate, ',')) {
			return NULL;
		}

		limit = parse_exp(pstate);
		if (NULL == limit) {
			return NULL;
		}

		if (ps_match(pstate, ',')) {
			step = parse_exp(pstate);
			if (NULL == step) {
				return NULL;
			}
		}

		if (NULL == ps_expect(pstate, TK_DO)) {
			return NULL;
		}

		blk = parse_block(pstate);
		if (NULL == blk) {
			return NULL;
		}

		if (NULL == ps_expect(pstate, TK_END)) {
			return NULL;
		}

		stat = mclast_alloc(PN_FORNUM, 4, pstate->heap);
		if (NULL == stat) {
			return NULL;
		}

		AST_VAL(stat).varname = first_name->semantics.varname;
		mclast_push_child(stat, start, pstate->heap);
		mclast_push_child(stat, limit, pstate->heap);
		if (NULL != step) {
			mclast_push_child(stat, step, pstate->heap);
		}

		mclast_push_child(stat, blk, pstate->heap);
		return stat;
	} else {
		stat = mclast_alloc(PN_FORIN, 4, pstate->heap);
		if (NULL == stat) {
			return NULL;
		}

		nm = mclast_alloc(PN_NAME, 0, pstate->heap);
		if (NULL == nm) {
			return NULL;
		}
		AST_VAL(nm).varname = first_name->semantics.varname;
		if (0 != mclast_push_child(stat, nm, pstate->heap)) {
			return NULL;
		}

		while (ps_match(pstate, ',')) {
			n = ps_expect(pstate, TK_NAME);
			if (NULL == n) {
				return NULL;
			}

			nm = mclast_alloc(PN_NAME, 0, pstate->heap);
			if (NULL == nm) {
				return NULL;
			}

			AST_VAL(nm).varname = n->semantics.varname;
			if (0 != mclast_push_child(stat, nm, pstate->heap)) {
				return NULL;
			}
		}

		AST_VAL(stat).integer = (i64) AST_NCHILD(stat);

		if (NULL == ps_expect(pstate, TK_IN)) {
			return NULL;
		}

		el = parse_explist(pstate);
		if (NULL == el) {
			return NULL;
		}

		if (0 != drain_children(pstate, stat, el)) {
			return NULL;
		}

		if (NULL == ps_expect(pstate, TK_DO)) {
			return NULL;
		}

		blk = parse_block(pstate);
		if (NULL == blk) {
			return NULL;
		}

		if (NULL == ps_expect(pstate, TK_END)) {
			return NULL;
		}

		if (0 != mclast_push_child(stat, blk, pstate->heap)) {
			return NULL;
		}
		return stat;
	}
}

static HeapHeader* parse_repeat(ParseState* pstate) {
	HeapHeader* stat = NULL;
	HeapHeader* blk = NULL;
	HeapHeader* cond = NULL;

	ps_advance(pstate);

	blk = parse_block(pstate);
	if (NULL == blk) {
		return NULL;
	}

	if (NULL == ps_expect(pstate, TK_UNTIL)) {
		return NULL;
	}

	cond = parse_exp(pstate);
	if (NULL == cond) {
		return NULL;
	}

	stat = mclast_alloc(PN_REPEAT, 2, pstate->heap);
	if (NULL == stat) {
		return NULL;
	}

	mclast_push_child(stat, blk, pstate->heap);
	mclast_push_child(stat, cond, pstate->heap);
	return stat;
}

static HeapHeader* parse_local(ParseState* pstate) {
	HeapHeader* stat = NULL;
	HeapHeader* body = NULL;
	HeapHeader* an = NULL;
	HeapHeader* el = NULL;
	Token* name_tok = NULL;

	ps_advance(pstate);

	if (TK_FUNCTION == CURTKN(pstate).token_number) {
		ps_advance(pstate);
		name_tok = ps_expect(pstate, TK_NAME);
		if (NULL == name_tok) {
			return NULL;
		}

		body = parse_funcbody(pstate);
		if (NULL == body) {
			return NULL;
		}

		stat = mclast_alloc(PN_LOCAL_FUNC, 1, pstate->heap);
		if (NULL == stat) {
			return NULL;
		}

		AST_VAL(stat).varname = name_tok->semantics.varname;
		mclast_push_child(stat, body, pstate->heap);
		return stat;
	}

	stat = mclast_alloc(PN_LOCAL, 4, pstate->heap);
	if (NULL == stat) {
		return NULL;
	}

	an = parse_attname(pstate);
	if (NULL == an) {
		return NULL;
	}

	if (0 != mclast_push_child(stat, an, pstate->heap)) {
		return NULL;
	}

	while (ps_match(pstate, ',')) {
		an = parse_attname(pstate);
		if (NULL == an) {
			return NULL;
		}

		if (0 != mclast_push_child(stat, an, pstate->heap)) {
			return NULL;
		}
	}

	AST_VAL(stat).integer = (i64) AST_NCHILD(stat);
	if (ps_match(pstate, '=')) {
		el = parse_explist(pstate);
		if (NULL == el) {
			return NULL;
		}

		if (0 != drain_children(pstate, stat, el)) {
			return NULL;
		}
	}
	return stat;
}

static HeapHeader* parse_goto(ParseState* pstate) {
	HeapHeader* stat = NULL;
	Token* name_tok = NULL;

	ps_advance(pstate);

	name_tok = ps_expect(pstate, TK_NAME);
	if (NULL == name_tok) {
		return NULL;
	}

	stat = mclast_alloc(PN_GOTO, 0, pstate->heap);
	if (NULL == stat) {
		return NULL;
	}

	AST_VAL(stat).varname = name_tok->semantics.varname;
	return stat;
}

static HeapHeader* parse_label(ParseState* pstate) {
	HeapHeader* stat = NULL;
	Token* name_tok = NULL;

	ps_advance(pstate);

	name_tok = ps_expect(pstate, TK_NAME);
	if (NULL == name_tok) {
		return NULL;
	}

	if (NULL == ps_expect(pstate, TK_DBCOLON)) {
		return NULL;
	}

	stat = mclast_alloc(PN_LABEL, 0, pstate->heap);
	if (NULL == stat) {
		return NULL;
	}

	AST_VAL(stat).varname = name_tok->semantics.varname;
	return stat;
}

static HeapHeader* parse_exprstat(ParseState* pstate) {
	HeapHeader* stat = NULL;
	HeapHeader* expr = NULL;
	HeapHeader* var = NULL;
	HeapHeader* el = NULL;

	expr = parse_suffixedexp(pstate);
	if (NULL == expr) {
		return NULL;
	}

	if (('=' == CURTKN(pstate).token_number)
			|| (',' == CURTKN(pstate).token_number)) {
		stat = mclast_alloc(PN_ASSIGN, 4, pstate->heap);
		if (NULL == stat) {
			return NULL;
		}

		if (0 != mclast_push_child(stat, expr, pstate->heap)) {
			return NULL;
		}

		while (ps_match(pstate, ',')) {
			var = parse_suffixedexp(pstate);
			if (NULL == var) {
				return NULL;
			}

			if (0 != mclast_push_child(stat, var, pstate->heap)) {
				return NULL;
			}
		}

		AST_VAL(stat).integer = (i64) AST_NCHILD(stat);
		if (NULL == ps_expect(pstate, '=')) {
			return NULL;
		}

		el = parse_explist(pstate);
		if (NULL == el) {
			return NULL;
		}

		if (0 != drain_children(pstate, stat, el)) {
			return NULL;
		}
		return stat;
	}

	if ((PN_CALL != AST_TYPE(expr)) && (PN_CALL_METHOD != AST_TYPE(expr))) {
		printf("parse error: expression is not a valid statement at pos %lu\n",
				pstate->pos);
		pstate->error = 1;
		return NULL;
	}

	stat = mclast_alloc(PN_CALL_STAT, 1, pstate->heap);
	if (NULL == stat) {
		return NULL;
	}

	mclast_push_child(stat, expr, pstate->heap);
	return stat;
}

static HeapHeader* parse_stat(ParseState* pstate) {
	switch (CURTKN(pstate).token_number) {
		case TK_IF:       return parse_if(pstate);
		case TK_WHILE:    return parse_while(pstate);
		case TK_DO:       return parse_do(pstate);
		case TK_FOR:      return parse_for(pstate);
		case TK_REPEAT:   return parse_repeat(pstate);
		case TK_FUNCTION: return parse_function(pstate, 0);
		case TK_LOCAL:    return parse_local(pstate);
		case TK_GOTO:     return parse_goto(pstate);
		case TK_BREAK:    ps_advance(pstate); return mclast_alloc(PN_BREAK, 0, pstate->heap);
		case TK_DBCOLON:  return parse_label(pstate);
		default:          return parse_exprstat(pstate);
	}
}

static HeapHeader* parse_block(ParseState* pstate) {
	HeapHeader* block = NULL;
	HeapHeader* ret = NULL;
	HeapHeader* st = NULL;

	block = mclast_alloc(PN_BLOCK, 4, pstate->heap);
	if (NULL == block) {
		return NULL;
	}

	while (!is_block_end(CURTKN(pstate).token_number)) {
		if (';' == CURTKN(pstate).token_number) {
			ps_advance(pstate);
			continue;
		}

		if (TK_RETURN == CURTKN(pstate).token_number) {
			ret = parse_retstat(pstate);
			if (NULL == ret) {
				return NULL;
			}
			if (0 != mclast_push_child(block, ret, pstate->heap)) {
				return NULL;
			}
			ps_match(pstate, ';');
			break;
		}

		st = parse_stat(pstate);
		if (pstate->error) {
			return NULL;
		}

		if (NULL == st) {
			continue;
		}

		if (0 != mclast_push_child(block, st, pstate->heap)) {
			return NULL;
		}
	}

	return block;
}

i8 mcparse_init(ParseState* pstate, MCHeap* heap) {
	if ((NULL == pstate) || (NULL == heap)) {
		return -1;
	}

	pstate->tokens = NULL;
	pstate->heap = heap;
	pstate->pos = 0;
	pstate->error = 0;
	pstate->root = NULL;

	return 0;
}

i8 mcparse_run(ParseState* pstate, TokenArray* tokens) {
	pstate->root = mcparse_parseroot(pstate, tokens);
	if (NULL != pstate->root) {
		return 0;
	}
	return -1;
}

void mcparse_free(ParseState* pstate) {
	if (NULL == pstate) {
		return;
	}
	pstate->root = NULL;
}

static HeapHeader* parse_function(ParseState* pstate, u8 is_main) {
	HeapHeader* newfunc = NULL;
	HeapHeader* name = NULL;
	HeapHeader* arglist = NULL;
	HeapHeader* block = NULL;
	i8 rcode = 0;

	newfunc = mclast_alloc(PN_FUNCDEF, 3, pstate->heap);
	name = mclast_alloc(PN_NAME, 2, pstate->heap);
	arglist = mclast_alloc(PN_ARGLIST, 4, pstate->heap);

	if ((NULL == newfunc) || (NULL == name) || (NULL == arglist)) {
		return NULL;
	}

	mclast_push_child(newfunc, name, pstate->heap);
	mclast_push_child(newfunc, arglist, pstate->heap);

	if (is_main) {
		AST_VAL(name).varname = (char*) "main";
		AST_VAL(arglist).integer = 1;
	} else {
		ps_advance(pstate);
		rcode = parse_funcname_into(pstate, name);
		if (rcode < 0) {
			return NULL;
		}
		AST_VAL(newfunc).integer = rcode;
		if (NULL == ps_expect(pstate, '(')) {
			return NULL;
		}
		if (0 != parse_arglist(pstate, arglist)) {
			return NULL;
		}
		if (NULL == ps_expect(pstate, ')')) {
			return NULL;
		}
	}

	block = parse_block(pstate);
	if ((pstate->error) || (NULL == block)) {
		return NULL;
	}
	mclast_push_child(newfunc, block, pstate->heap);

	if (!is_main) {
		if (NULL == ps_expect(pstate, TK_END)) {
			return NULL;
		}
	}

	return newfunc;
}

static HeapHeader* mcparse_parseroot(ParseState* pstate, TokenArray* tokens) {
	pstate->tokens = tokens;
	pstate->pos = 0;
	pstate->error = 0;
	return parse_function(pstate, 1);
}

static const char* nodestr[] = {
	"nil", "true", "false", "integer", "float", "string", "...", "name",
	"binop", "unop", "index", "field", "call", "call_method", "func_expr", "table",
	"field_idx", "field_name", "field_val",
	"explist", "namelist", "varlist",
	"funcbody", "arglist", "attname",
	"block", "assign", "local", "do", "while", "repeat",
	"if", "elseif", "else", "fornum", "forin",
	"funcdef", "local_func", "return", "break", "goto", "label", "call_stat",
};

#define PN2STR(type) (nodestr[(type)])

void mcparse_log_node(HeapHeader* node, i32 depth) {
	if (NULL == node) {
		return;
	}

	for (i32 tabidx = 0; tabidx < depth; tabidx += 1) {
		printf("  ");
	}

	printf("[%s]", PN2STR(AST_TYPE(node)));

	switch (AST_TYPE(node)) {
		case PN_INTEGER:
			printf(" %ld", AST_VAL(node).integer);
			break;
		case PN_FLOAT:
			printf(" %f", AST_VAL(node).number);
			break;
		case PN_STRING:
			printf(" %s", mclstr_getchars(AST_VAL(node).heapobj));
			break;
		case PN_NAME:
		case PN_GOTO:
		case PN_LABEL:
		case PN_FIELD:
		case PN_CALL_METHOD:
		case PN_LOCAL_FUNC:
		case PN_ATTNAME:
		case PN_FORNUM:
			printf(" %s", AST_VAL(node).varname);
			break;
		case PN_BINOP:
		case PN_UNOP:
			if (AST_VAL(node).integer < FIRST_RESERVED) {
				printf(" '%c'", (char) AST_VAL(node).integer);
			} else {
				printf(" %s", TK2STR((i32) AST_VAL(node).integer));
			}
			break;
		case PN_ARGLIST:
		case PN_FUNCDEF:
			if (AST_VAL(node).integer) {
				printf(" (vararg/method)");
			}
			break;
		case PN_ASSIGN:
		case PN_LOCAL:
		case PN_FORIN:
			printf(" name_count=%ld", AST_VAL(node).integer);
			break;
		default:
			break;
	}
	printf("\n");

	for (u32 idx = 0; idx < AST_NCHILD(node); idx++) {
		mcparse_log_node(AST_CHILD(node, idx), depth + 1);
	}
}
