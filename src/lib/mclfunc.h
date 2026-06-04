#pragma once

#include "mctoken.h"
#include "mcheap.h"

typedef enum ConstType {
    K_INT,
    K_FLT,
    K_STR
} ConstType;

typedef struct Constant {
    ConstType type;
    SemanticInfo val;
} Constant;




typedef struct UpvalDesc {
    char* name;
    u8 instack;
    u8 idx;
} UpvalDesc;

typedef struct mclfunc {
    char* name;
    heap_header* constants;
    heap_header* protos;
    heap_header* upvals;
    u8 num_params;
    u8 is_vararg;
    u8 num_upvals;
    u8 max_stack;
    u32 code_len;
    u32 num_consts;
    u32 num_protos;
} mclfunc;

#define FUNCLEN(cstate) ((cstate)->func->object.function->code_len)

heap_header* mclfunc_alloc(u32 code_cap, MCHeap* heap);

u32* mclfunc_getbody(heap_header* func);
i8 mclfunc_append_instr(heap_header* func, u32 instr, MCHeap* heap);

Constant* mclfunc_getconsts(heap_header* func);
heap_header** mclfunc_getprotos(heap_header* func);
UpvalDesc* mclfunc_getupvals(heap_header* func);

i32 mclfunc_const_int(heap_header* func, i64 value, MCHeap* heap);
i32 mclfunc_const_flt(heap_header* func, f64 value, MCHeap* heap);
i32 mclfunc_const_str(heap_header* func, heap_header* str, MCHeap* heap);
i32 mclfunc_add_proto(heap_header* func, heap_header* proto, MCHeap* heap);
i32 mclfunc_add_upval(heap_header* func, char* name, u8 instack, u8 idx,
        MCHeap* heap);

void mclfunc_clear(heap_header* func);
