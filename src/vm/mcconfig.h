#pragma once

#include "mctypes.h"

typedef struct VMConfig {
    u64 MAX_MEM;
    u64 MAX_RUNTIME_MEM;

    u64 MAX_FILE_SIZE;
    u64 MAX_PROGRAM_SIZE;

    u64 MAX_TOKENS;

    u64 MAX_IDLEN;
    u64 MAX_NUMLEN;
    u64 MAX_TERMLEN;
    u64 MAX_STR_LEN;

    u64 MAX_VARNAME_ENTRIES;
    u64 MAX_VARNAME_POOL_SIZE;

    i8 DEBUG_MODE;
} VMConfig;

i8 mcvm_validate_config(VMConfig* cfg);
i8 mcvm_set_default_config(VMConfig* cfg);
