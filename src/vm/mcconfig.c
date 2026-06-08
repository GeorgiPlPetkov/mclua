#include <stdio.h>

#include "mctypes.h"

#include "mcconfig.h"

i8 mcvm_set_default_config(VMConfig* cfg) {
    if (NULL == cfg) {
        return -1;
    }

    cfg->MAX_MEM = 256 * 1024 * 1024;

    cfg->MAX_FILE_CHUNK_SIZE = 1024;

    cfg->MAX_TOKENS = 16384;
    cfg->MAX_IDLEN = 128;
    cfg->MAX_NUMLEN = 32;
    cfg->MAX_TERMLEN = 4;

    cfg->MIN_STR_LEN = 64;
    cfg->MAX_STR_LEN = 16384;
    cfg->MIN_FUNC_LEN = 32;
    cfg->MAX_FUNC_LEN = 64;

    cfg->MAX_NEST_DEPTH = 256;
    cfg->MAX_EXT_VISITORS = 8;

    cfg->MAX_LEX_MEM = cfg->MAX_TOKENS * sizeof(Token)
            + cfg->MAX_IDLEN + cfg->MAX_FILE_CHUNK_SIZE;

    cfg->MAX_VARNAME_ENTRIES = 1024;
    cfg->MAX_VARNAME_POOL_SIZE = cfg->MAX_VARNAME_ENTRIES * cfg->MAX_IDLEN;

    cfg->MAX_LOCALS = 256;
    cfg->MAX_BREAKS = 64;

    return 0;
}

i8 mcvm_validate_config(VMConfig* cfg) {
    if (NULL == cfg) {
        return -1;
    }

    if (cfg->MAX_LEX_MEM <= cfg->MAX_IDLEN) {
        printf("config: MAX_LEX_MEM must be larger than MAX_IDLEN\n");
        return -1;
    }

    if (0 == cfg->MAX_IDLEN) {
        printf("config: MAX_IDLEN cannot be 0\n");
        return -1;
    }

    if (0 == cfg->MAX_STR_LEN) {
        printf("config: MAX_STR_LEN cannot be 0\n");
        return -1;
    }

    if (0 == cfg->MAX_VARNAME_ENTRIES) {
        printf("config: MAX_VARNAME_ENTRIES cannot be 0\n");
        return -1;
    }

    if (0 == cfg->MAX_VARNAME_POOL_SIZE) {
        printf("config: MAX_VARNAME_POOL_SIZE cannot be 0\n");
        return -1;
    }

    if (0 == cfg->MAX_LOCALS) {
        printf("config: MAX_LOCALS cannot be 0\n");
        return -1;
    }

    if (0 == cfg->MAX_BREAKS) {
        printf("config: MAX_BREAKS cannot be 0\n");
        return -1;
    }

    if (cfg->MAX_IDLEN > cfg->MAX_STR_LEN) {
        printf("config: MAX_IDLEN should not exceed MAX_STR_LEN\n");
        return -1;
    }

    return 0;
}
