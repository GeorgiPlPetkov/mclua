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

heap_header* mcheap_managed_reserve(MCHeap* heap, u32 cap) {
    if (NULL == heap) {
        return NULL;
    }

    if ((heap->data_top + cap + sizeof(heap_header)) > heap->head_bot) {
        return NULL;
    }

    byte* objptr = heap->bfr + heap->data_top;
    heap->data_top += cap;
    heap->head_bot -= sizeof(heap_header);
    heap->managed_cap -= (cap + sizeof(heap_header));
    heap_header* hdr = (heap_header*)(heap->bfr + heap->head_bot);

    hdr->object.rawdata = objptr;
    hdr->objcap = cap;
    heap->hdr_cnt += 1;

    return hdr;
}

i8 mcheap_managed_resize(MCHeap* heap, heap_header* hdr, u32 newlen) {
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
    memcpy(new_data, (byte*) hdr->object.rawdata, oldcap);
    heap->data_top += newlen;
    heap->managed_cap -= (newlen - oldcap);

    hdr->object.rawdata = new_data;
    hdr->objcap = newlen;

    return 0;
}

void mcheap_logstate(MCHeap* heap) {
    printf("mcheap: %u hdrs, data=%lu/%lu, hdrs=%lu/%lu, free=%lu\n",
           heap->hdr_cnt,
           heap->data_top, heap->head_bot,
           (heap->bfrcap - heap->head_bot), heap->bfrcap,
           heap->head_bot - heap->data_top);

    heap_header* cur = heap->header_root;
    u32 idx = 0;
    while (NULL != cur) {
        printf("[%u]: cap=%u\n", idx, cur->objcap);
        cur = cur->next;
        idx += 1;
    }
}
