#pragma once

#include <string.h>

#include "mctypes.h"

typedef struct str8 {
    char* content;
    u64   length;
    u64   capacity;
} str8;

i8 str8_alloc(str8* str, u64 cap);
void str8_free(str8* str);

i8 str8_attach_nt(str8* str, const char* str0);
const char* str8_dattach_nt(str8* str);

i8 str8_reserve(str8* str, u64 cap, char* bfr);
i8 str8_appendnt(str8* str, char* ntstr);
i8 str8_appendstr(str8* str1, const str8* str2);
i8 str8_copynt(str8* str, char* ntstr);
i8 str8_copystr(str8* dst, const str8* src);
