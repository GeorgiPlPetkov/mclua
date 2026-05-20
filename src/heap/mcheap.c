#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mctypes.h"

#include "mcheap.h"

i8 mcheap_init(MCHeap* heap, u64 cap) {
    byte* bfr = (byte*) calloc(cap, 1);
    if (NULL == bfr) {
        return -1;
    }

    heap->bfr = bfr;
    heap->bfrcap = cap;

    heap->data_top = 0;
    heap->managed_cap = cap;
    heap->head_bot = cap;
    heap->hdr_cnt = 0;

    return 0;
}

void mcheap_free(MCHeap* heap) {
    if (NULL != heap->bfr) {
        free(heap->bfr);
        heap->bfr = NULL;
    }
}

byte* mcheap_reserve(MCHeap* heap, u64 len) {
    byte* data = NULL;

    if (NULL == heap) {
        return NULL;
    }

    if ((heap->data_top + len) > heap->bfrcap) {
        return NULL;
    }
    data = heap->bfr + heap->data_top;
    heap->data_top += len;
    heap->managed_cap -= len;

    return data;
}

heap_hdr* mcheap_managed_reserve(MCHeap* heap, u32 len);
heap_hdr* mcheap_managed_emplace(MCHeap* heap, byte* data, u32 len);
i8 mcheap_managed_resize(MCHeap* heap, heap_hdr* hdr, u32 newlen);

void mcheap_logstate(MCHeap* heap) {
    printf("mcheap: %u hdrs, data=%lu/%lu, hdrs=%lu/%lu, free=%lu\n",
           heap->hdr_cnt,
           heap->data_top, heap->head_bot,
           heap->bfrcap - heap->head_bot, heap->bfrcap,
           heap->head_bot - heap->data_top);

    heap_hdr* cur = heap->head;
    u32 idx = 0;
    while (NULL != cur) {
        printf("[%u]: len=%u cap=%u \"%.*s\"\n",
               idx, cur->len, cur->cap,
               cur->len, (char*) cur->data);
        cur = cur->next;
        idx += 1;
    }
}
