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
    if ((NULL != heap) && (NULL != heap->bfr)) {
        free(heap->bfr);
        heap->bfr = NULL;
    }
}

byte* mcheap_static_reserve(MCHeap* heap, u64 len) {
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

HeapHeader* mcheap_managed_reserve(MCHeap* heap, u32 cap) {
    if (NULL == heap) {
        return NULL;
    }

    if ((heap->data_top + cap + sizeof(HeapHeader)) > heap->head_bot) {
        return NULL;
    }

    byte* objptr = heap->bfr + heap->data_top;
    heap->data_top += cap;
    heap->head_bot -= sizeof(HeapHeader);
    heap->managed_cap -= (cap + sizeof(HeapHeader));
    HeapHeader* hdr = (HeapHeader*)(heap->bfr + heap->head_bot);

    hdr->object.data = objptr;
    hdr->objcap = cap;
    heap->hdr_cnt += 1;

    return hdr;
}

i8 mcheap_managed_resize(MCHeap* heap, HeapHeader* hdr, u32 newlen) {
    u32 oldcap = 0;

    if ((NULL == heap) || (NULL == hdr)) {
        return -1;
    }

    if (newlen <= hdr->objcap) {
        return 0;
    }

    if ((heap->data_top + newlen) > heap->head_bot) {
        return -1; /* OOM */
    }

    oldcap = hdr->objcap;

    byte* new_data = heap->bfr + heap->data_top;
    memcpy(new_data, (byte*) hdr->object.data, oldcap);
    heap->data_top += newlen;
    heap->managed_cap -= (newlen - oldcap);

    hdr->object.data = new_data;
    hdr->objcap = newlen;

    return 0;
}

void mcheap_logstate(MCHeap* heap) {
    printf("mcheap: %u hdrs, data=%lu/%lu, hdrs=%lu/%lu, free=%lu\n",
           heap->hdr_cnt,
           heap->data_top, heap->head_bot,
           (heap->bfrcap - heap->head_bot), heap->bfrcap,
           heap->head_bot - heap->data_top);

    HeapHeader* cur = heap->header_root;
    u32 idx = 0;
    while (NULL != cur) {
        printf("[%u]: cap=%u\n", idx, cur->objcap);
        cur = cur->next;
        idx += 1;
    }
}
