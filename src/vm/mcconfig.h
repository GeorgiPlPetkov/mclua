#pragma once

#include "mctypes.h"
#include "mctoken.h"

typedef struct VMConfig {
    u64 MAX_MEM;
    u64 MAX_FILE_CHUNK_SIZE;

    u64 MAX_LEX_MEM;
    u64 MAX_TOKENS;

    u64 MAX_IDLEN;
    u64 MAX_NUMLEN;
    u64 MAX_TERMLEN;

    u32 MIN_STR_LEN;
    u32 MAX_STR_LEN;
    u32 MIN_FUNC_LEN;
    u32 MAX_FUNC_LEN;

    u64 MAX_VARNAME_ENTRIES;
    u64 MAX_VARNAME_POOL_SIZE;

    u32 MAX_NEST_DEPTH;
    u32 MAX_EXT_VISITORS;
    u32 MAX_LOCALS;
    u32 MAX_BREAKS;
} VMConfig;

i8 mcvm_validate_config(VMConfig* cfg);
i8 mcvm_set_default_config(VMConfig* cfg);
