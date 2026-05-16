#include <stdio.h>

#include "mctypes.h"

#include "mcconfig.h"

i8 mcvm_set_default_config(VMConfig* cfg) {
    if (NULL == cfg) {
        return -1;
    }

    cfg->MAX_MEM = 1024 * 1024;
    cfg->MAX_RUNTIME_MEM = 1024 * 1024;

    cfg->MAX_FILE_SIZE = 1024;
    cfg->MAX_PROGRAM_SIZE = 1024 * 1024;

    cfg->MAX_TOKENS = 1024;

    cfg->MAX_IDLEN = 64;
    cfg->MAX_NUMLEN = 32;
    cfg->MAX_TERMLEN = 4;
    cfg->MAX_STR_LEN = 1024;

    cfg->MAX_VARNAME_ENTRIES = 1024;
    cfg->MAX_VARNAME_POOL_SIZE = 1024 * 32;

    cfg->DEBUG_MODE = 0;

    return 0;
}

i8 mcvm_validate_config(VMConfig* cfg) {
    if (NULL == cfg) {
        return -1;
    }

    if (0 == cfg->MAX_TOKENS) {
        printf("config: MAX_TOKENS cannot be 0\n");
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

    if (cfg->MAX_IDLEN > cfg->MAX_STR_LEN) {
        printf("config: MAX_IDLEN should not exceed MAX_STR_LEN\n");
        return -1;
    }

    return 0;
}
