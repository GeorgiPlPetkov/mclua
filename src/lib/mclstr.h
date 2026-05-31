#pragma once

#include "mcheap.h"

typedef struct mclstr {
    u32 len;
} mclstr;

heap_header* mclstr_alloc(u32 len, MCHeap* heap);
i8 mclstr_append_lstr(heap_header* dst, heap_header* src, MCHeap* heap);
i8 mclstr_append_str0(heap_header* dst, const char* str0, u32 str0len, MCHeap* heap);
heap_header* mclstr_sub(u32 start, u32 end, heap_header* s, MCHeap* heap);
heap_header* mclstr_rep(u32 n, heap_header* s, MCHeap* heap);

char* mclstr_getchars(heap_header* str);
u8 mclstr_byte(heap_header* str, u32 idx);
i8 mclstr_cmp(heap_header* str1, heap_header* str2);
i8 mclstr_ptrcmp(heap_header* str1, heap_header* str2);

void mclstr_upper(heap_header* str);
void mclstr_lower(heap_header* str);
void mclstr_reverse(heap_header* str);
void mclstr_clear(heap_header* str);
