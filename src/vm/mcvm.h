#pragma once

#include "mcconfig.h"
#include "mcstrtbl.h"
#include "mclex.h"

#include "mcstr.h"

typedef struct gstate {
    VMConfig config;
    byte* mem;
    StringTable stringtable;
    LexState lexstate;
} VMState;

i8 mcvm_init(VMState* vm, VMConfig* cfg);
void mcvm_free(VMState* vm);

i8 mcvm_parse_str8(VMState* vm, str8* str);
i8 mcvm_parse_str0(VMState* vm, const char* str);
i8 mcvm_parsefile(VMState* vm, const char* path);
