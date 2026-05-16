#pragma once

#include "mctypes.h"

typedef struct VMConfig {
    u64 MAX_MEM;
    u64 MAX_FILE_SIZE;
    u64 MAX_PROGRAM_SIZE;
} VMConfig;
