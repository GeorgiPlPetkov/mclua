#pragma once

#include "mctypes.h"

typedef struct heap_hdr {
    struct heap_hdr* next;
    byte* data;
    u32 len;
    u32 cap;
} heap_hdr;

typedef struct {
    u8* bfr;
    u64 bfrcap;
    u64 managed_cap;
    u64 data_top; /* next free byte from front */
    u64 head_bot;  /* first header byte from back */
    heap_hdr* head;
    u32 hdr_cnt;
} MCHeap;

i8 mcheap_init(MCHeap* heap, u64 cap);
void mcheap_free(MCHeap* heap);

byte* mcheap_reserve(MCHeap* heap, u64 len);

heap_hdr* mcheap_managed_reserve(MCHeap* heap, u32 len);
heap_hdr* mcheap_managed_emplace(MCHeap* heap, byte* data, u32 len);
i8 mcheap_managed_resize(MCHeap* heap, heap_hdr* hdr, u32 newlen);

void mcheap_logstate(MCHeap* heap);
