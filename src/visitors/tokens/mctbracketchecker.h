#pragma once

#include "mctypes.h"
#include "mctokenvisitor.h"

typedef struct BracketChecker {
	i32* partners;
	i32 stack[256];
	i32 top;
	u32 max_depth;
	u64 token_count;
} BracketChecker;

void mctbc_setup(TokenVisitor* visitor, BracketChecker* checker,
		i32* partners, u32 max_depth);
