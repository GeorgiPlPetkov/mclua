#pragma once

#include "mclex.h"
#include "mcstr.h"

typedef struct vmcof {
    u64 MAX_MEM;
    u64 MAX_FILE_SIZE;
} VMConfig;

typedef struct gstate {
    VMConfig config;
    LexState lexstate;
} VMState;

i8 mcvm_init(VMState* vm, VMConfig* cfg);
void mcvm_free(VMState* vm);

i8 mcvm_parsestr8(VMState* vm, str8* str);
i8 mcvm_parsestr0(VMState* vm, const char* str);
i8 mcvm_parsefile(VMState* vm, const char* path);
