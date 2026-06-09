#pragma once

#include "mctypes.h"

typedef enum type {
    OBJ_STR  = 0,
    OBJ_TBL  = 1,
    OBJ_USR  = 2,
    OBJ_FUNC = 3,
    OBJ_AST  = 4
} ObjectType;

struct mclstr;
struct mclfunc;
struct mclastnode;

typedef struct HeapHeader {
    struct HeapHeader* next;
    union {
        struct mclstr* string;
        struct mclfunc* function;
        struct mclastnode* astnode;
        byte* data;
    } object;
    u32 objcap;
    ObjectType type;
} HeapHeader;

typedef struct mcheap {
    u8* bfr;
    u64 bfrcap;
    u64 managed_cap;
    u64 data_top; /* next free byte from front */
    u64 head_bot;  /* first header byte from back */
    HeapHeader* header_root;
    u32 hdr_cnt;
} MCHeap;

i8 mcheap_init(MCHeap* heap, u64 cap);
void mcheap_free(MCHeap* heap);

byte* mcheap_static_reserve(MCHeap* heap, u64 len);

HeapHeader* mcheap_managed_reserve(MCHeap* heap, u32 cap);
i8 mcheap_managed_resize(MCHeap* heap, HeapHeader* hdr, u32 newlen);

void mcheap_logstate(MCHeap* heap);
