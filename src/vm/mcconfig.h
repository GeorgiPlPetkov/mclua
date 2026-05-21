#pragma once

#include "mctypes.h"

#define TOKEN_SIZE (16)

typedef struct VMConfig {
    u64 MAX_MEM;
    u64 MAX_FILE_SIZE;

    u64 MAX_LEX_MEM;
    u64 MAX_TOKENS;

    u64 MAX_IDLEN;
    u64 MAX_NUMLEN;
    u64 MAX_TERMLEN;
    u64 MIN_STR_LEN;
    u64 MAX_STR_LEN;

    u64 MAX_VARNAME_ENTRIES;
    u64 MAX_VARNAME_POOL_SIZE;
} VMConfig;

i8 mcvm_validate_config(VMConfig* cfg);
i8 mcvm_set_default_config(VMConfig* cfg);
