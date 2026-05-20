#include <stdio.h>
#include <string.h>

#include "mctypes.h"

#include "mcstrtbl.h"

static u32 mcstrtbl_hash(const char* str, u32 len);

i8 mcstrtbl_init(StringTable* tbl, u32 maxvars, byte* bfr, u64 bfrcap) {
    u64 entries_size = maxvars * sizeof(strtbl_entry);
    if ((NULL == bfr) || (entries_size >= bfrcap)) {
        return -1;
    }

    tbl->entries = (strtbl_entry*) bfr;

    tbl->entry_cap = maxvars;
    tbl->entry_cnt = 0;

    tbl->bfr = bfr + entries_size;
    tbl->bfrcap = bfrcap - entries_size;
    tbl->bfr_used = 0;

    return 0;
}

void mcstrtbl_clear(StringTable* tbl) {
    memset(tbl->entries, 0, tbl->entry_cap * sizeof(strtbl_entry));
    tbl->entry_cnt = 0;
    tbl->bfr_used = 0;
}

char* mcstrtbl_intern(StringTable* tbl, const char* str, u32 len) {
    char* existing = NULL;
    char* dest = NULL;
    u32 hash = 0;
    u32 idx = 0;

    if ((NULL == tbl) || (NULL == str) || (len == 0)) {
        return NULL;
    }

    existing = mcstrtbl_lookup(tbl, str, len);
    if (NULL != existing) {
        return existing;
    }

    if (tbl->entry_cnt >= tbl->entry_cap) {
        return NULL;
    }

    if ((tbl->bfr_used + len + 1) > tbl->bfrcap) {
        return NULL;
    }

    dest = (char*) tbl->bfr + tbl->bfr_used;
    memcpy(dest, str, len);
    dest[len] = '\0';
    tbl->bfr_used += len + 1;

    hash = mcstrtbl_hash(str, len);
    idx = hash % tbl->entry_cap;

    while (NULL != tbl->entries[idx].str) {
        idx = (idx + 1) % tbl->entry_cap;
    }

    tbl->entries[idx].str = dest;
    tbl->entries[idx].len = len;
    tbl->entries[idx].hash = hash;
    tbl->entry_cnt += 1;

    return dest;
}

char* mcstrtbl_lookup(StringTable* tbl, const char* str, u32 len) {
    u32 hash = mcstrtbl_hash(str, len);
    u32 idx = hash % tbl->entry_cap;

    while (NULL != tbl->entries[idx].str) {
        if ((hash == tbl->entries[idx].hash)
            && (len == tbl->entries[idx].len)
            && (0 == memcmp(tbl->entries[idx].str, str, len))) {
            return tbl->entries[idx].str;
        }

        idx = (idx + 1) % tbl->entry_cap;
    }

    return NULL;
}

void mcstrtbl_logstate(StringTable* tbl) {
    printf("strtbl: %u/%u entries, %lu/%lu bytes\n",
           tbl->entry_cnt, tbl->entry_cap,
           tbl->bfr_used, tbl->bfrcap);

    for (u32 idx = 0; idx < tbl->entry_cap; idx += 1) {
        if (NULL == tbl->entries[idx].str) {
            continue;
        }

        printf("[%u]: hash=%08x len=%u \"%.*s\"\n",
               idx, tbl->entries[idx].hash,
               tbl->entries[idx].len,
               tbl->entries[idx].len, tbl->entries[idx].str);
    }
}

static u32 mcstrtbl_hash(const char* str, u32 len) {
    u32 hash = 2166136261u;
    for (u64 idx = 0; idx < len; idx += 1) {
        hash ^= (uchar) str[idx];
        hash *= 16777619u;
    }
    return hash;
}
