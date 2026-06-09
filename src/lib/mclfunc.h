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
    HeapHeader* constants;
    HeapHeader* const_index;
    HeapHeader* protos;
    HeapHeader* upvals;
    u8 num_params;
    u8 is_vararg;
    u8 num_upvals;
    u8 max_stack;
    u32 code_len;
    u32 num_consts;
    u32 num_protos;
    u32 const_index_cap;
} mclfunc;

#define FUNCLEN(cstate) ((cstate)->func->object.function->code_len)

HeapHeader* mclfunc_alloc(u32 code_cap, MCHeap* heap);

u32* mclfunc_getbody(HeapHeader* func);
i8 mclfunc_append_instr(HeapHeader* func, u32 instr, MCHeap* heap);

Constant* mclfunc_getconsts(HeapHeader* func);
HeapHeader** mclfunc_getprotos(HeapHeader* func);
UpvalDesc* mclfunc_getupvals(HeapHeader* func);

i32 mclfunc_const_int(HeapHeader* func, i64 value, MCHeap* heap);
i32 mclfunc_const_flt(HeapHeader* func, f64 value, MCHeap* heap);
i32 mclfunc_const_str(HeapHeader* func, HeapHeader* str, MCHeap* heap);
i32 mclfunc_add_proto(HeapHeader* func, HeapHeader* proto, MCHeap* heap);
i32 mclfunc_add_upval(HeapHeader* func, char* name, u8 instack, u8 idx,
        MCHeap* heap);

void mclfunc_clear(HeapHeader* func);
