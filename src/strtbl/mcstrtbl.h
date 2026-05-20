#pragma once

#include "mctypes.h"

typedef struct {
    char* str;
    u32   len;
    u32   hash;
} strtbl_entry;

typedef struct {
    strtbl_entry* entries;
    u32 entry_cap;
    u32 entry_cnt;

    byte* bfr;
    u64 bfrcap;
    u64 bfr_used;
} StringTable;

i8 mcstrtbl_init(StringTable* tbl, u32 maxvars, byte* bfr, u64 bfrcap);
void mcstrtbl_clear(StringTable* tbl);

char* mcstrtbl_intern(StringTable* tbl, const char* str, u32 len);
char* mcstrtbl_lookup(StringTable* tbl, const char* str, u32 len);

void mcstrtbl_logstate(StringTable* tbl);
