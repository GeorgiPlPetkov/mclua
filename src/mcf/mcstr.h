#pragma once

#include <string.h>

#include "mctypes.h"

typedef struct str8 {
    char* content;
    u64   lenght;
    u64   capacity;
} str8;

i8 str8_alloc(str8* str, u64 cap);
void str8_free(str8* str);

i8 str8_attach(str8* str, char* bfr);
