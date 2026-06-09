#pragma once

#include "mcheap.h"

typedef struct mclstr {
    u32 len;
} mclstr;

HeapHeader* mclstr_alloc(u32 len, MCHeap* heap);
i8 mclstr_append_lstr(HeapHeader* dst, HeapHeader* src, MCHeap* heap);
i8 mclstr_append_str0(HeapHeader* dst, const char* str0, u32 str0len, MCHeap* heap);
HeapHeader* mclstr_sub(u32 start, u32 end, HeapHeader* s, MCHeap* heap);
HeapHeader* mclstr_rep(u32 n, HeapHeader* s, MCHeap* heap);

char* mclstr_getchars(HeapHeader* str);
u8 mclstr_byte(HeapHeader* str, u32 idx);
i8 mclstr_cmp(HeapHeader* str1, HeapHeader* str2);
i8 mclstr_ptrcmp(HeapHeader* str1, HeapHeader* str2);

void mclstr_upper(HeapHeader* str);
void mclstr_lower(HeapHeader* str);
void mclstr_reverse(HeapHeader* str);
void mclstr_clear(HeapHeader* str);
