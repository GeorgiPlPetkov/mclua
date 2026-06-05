#include <stdio.h>

#include "mctypes.h"
#include "mclast.h"
#include "mcastvisitor.h"
#include "mcunreach.h"

static const char* terminator_name(ParseNodeType t) {
	if (PN_RETURN == t) {
		return "return";
	}
	if (PN_BREAK == t) {
		return "break";
	}
	return "goto";
}

static i8 unreach_visit(void* ctx, heap_header* node, u32 depth) {
	(void) ctx;
	(void) depth;

	if (PN_BLOCK != AST_TYPE(node)) {
		return 0;
	}

	u32 n = AST_NCHILD(node);
	heap_header** children = mclast_children(node);

	for (u32 idx = 0; (idx + 1) < n; idx += 1) {
		ParseNodeType t = AST_TYPE(children[idx]);
		if ((PN_RETURN == t) || (PN_BREAK == t) || (PN_GOTO == t)) {
			printf("[unreach] removed %u unreachable node(s) after %s\n",
				    (n - (idx + 1)), terminator_name(t));
			AST_NCHILD(node) = idx + 1;
			break;
		}
	}
	return 0;
}

static ASTVisitor visitor_instance = {
	.init   = NULL,
	.visit  = unreach_visit,
	.deinit = NULL,
	.ctx    = NULL,
};

ASTVisitor* mcastv_load(void) {
	return &visitor_instance;
}
