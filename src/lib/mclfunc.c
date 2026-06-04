#include <string.h>

#include "mctypes.h"
#include "mcmemops.h"

#include "mclfunc.h"
#include "mclstr.h"
#include "mcheap.h"

#define CONST_INIT_CAP (4)
#define PROTO_INIT_CAP (2)

heap_header* mclfunc_alloc(u32 bodycap, MCHeap* heap) {
    heap_header* hdr = mcheap_managed_reserve(heap,
            sizeof(mclfunc) + (bodycap * sizeof(u32)));
    if (NULL == hdr) {
        return NULL;
    }

    hdr->type = OBJ_FUNC;
    MemsetZeroStruct(hdr->object.function);

    return hdr;
}

u32* mclfunc_getbody(heap_header* func) {
    return (u32*)((byte*) func->object.function + sizeof(mclfunc));
}

i8 mclfunc_append_instr(heap_header* func, u32 instr, MCHeap* heap) {
    u32* code = NULL;
    u32 reqsize = 0;
    i8 rcode = 0;

    reqsize = sizeof(mclfunc)
            + ((func->object.function->code_len + 1) * sizeof(u32));
    if (reqsize > func->objcap) {
        rcode = mcheap_managed_resize(heap, func, reqsize);
        if (0 != rcode) {
            return -1;
        }
    }

    code = mclfunc_getbody(func);
    code[func->object.function->code_len] = instr;
    func->object.function->code_len += 1;

    return rcode;
}

Constant* mclfunc_getconsts(heap_header* func) {
    if (NULL == func->object.function->constants) {
        return NULL;
    }
    return (Constant*) func->object.function->constants->object.data;
}

heap_header** mclfunc_getprotos(heap_header* func) {
    if (NULL == func->object.function->protos) {
        return NULL;
    }
    return (heap_header**) func->object.function->protos->object.data;
}

UpvalDesc* mclfunc_getupvals(heap_header* func) {
    if (NULL == func->object.function->upvals) {
        return NULL;
    }
    return (UpvalDesc*) func->object.function->upvals->object.data;
}



static i8 array_ensure(heap_header** slot, u32 count, u32 elemsize,
        u32 initcap, MCHeap* heap) {
    u32 reqsize = (count + 1) * elemsize;
    u32 newcap = 0;

    if (NULL == *slot) {
        *slot = mcheap_managed_reserve(heap, initcap * elemsize);
        if (NULL == *slot) {
            return -1;
        }
        (*slot)->type = OBJ_USR;
        return 0;
    }

    if (reqsize > (*slot)->objcap) {
        newcap = (*slot)->objcap * 2;
        if (newcap < reqsize) {
            newcap = reqsize;
        }
        if (0 != mcheap_managed_resize(heap, *slot, newcap)) {
            return -1;
        }
    }

    return 0;
}

static i32 mclfunc_push_const(heap_header* func, Constant value, MCHeap* heap) {
    mclfunc* fn = func->object.function;
    Constant* arr = NULL;
    i32 idx = 0;

    if (0 != array_ensure(&fn->constants, fn->num_consts,
            sizeof(Constant), CONST_INIT_CAP, heap)) {
        return -1;
    }

    arr = (Constant*) fn->constants->object.data;
    arr[fn->num_consts] = value;
    idx = (i32) fn->num_consts;
    fn->num_consts += 1;

    return idx;
}

i32 mclfunc_const_int(heap_header* func, i64 value, MCHeap* heap) {
    Constant* arr = mclfunc_getconsts(func);
    Constant entry = {0, {0}};
    u32 idx = 0;

    for (idx = 0; idx < func->object.function->num_consts; idx += 1) {
        if ((K_INT == arr[idx].type) && (value == arr[idx].val.integer)) {
            return (i32) idx;
        }
    }

    entry.type = K_INT;
    entry.val.integer = value;
    return mclfunc_push_const(func, entry, heap);
}

i32 mclfunc_const_flt(heap_header* func, f64 value, MCHeap* heap) {
    Constant* arr = mclfunc_getconsts(func);
    Constant entry = {0, {0}};
    u32 idx = 0;

    for (idx = 0; idx < func->object.function->num_consts; idx += 1) {
        if ((K_FLT == arr[idx].type) && (value == arr[idx].val.number)) {
            return (i32) idx;
        }
    }

    entry.type = K_FLT;
    entry.val.number = value;
    return mclfunc_push_const(func, entry, heap);
}

i32 mclfunc_const_str(heap_header* func, heap_header* str, MCHeap* heap) {
    Constant* arr = mclfunc_getconsts(func);
    Constant entry = {0, {0}};
    u32 idx = 0;

    for (idx = 0; idx < func->object.function->num_consts; idx += 1) {
        if ((K_STR == arr[idx].type)
                && (0 == mclstr_cmp(str, arr[idx].val.heapobj))) {
            return (i32) idx;
        }
    }

    entry.type = K_STR;
    entry.val.heapobj = str;
    return mclfunc_push_const(func, entry, heap);
}

i32 mclfunc_add_proto(heap_header* func, heap_header* proto, MCHeap* heap) {
    mclfunc* fn = func->object.function;
    heap_header** arr = NULL;
    i32 idx = 0;

    if (0 != array_ensure(&fn->protos, fn->num_protos,
            sizeof(heap_header*), PROTO_INIT_CAP, heap)) {
        return -1;
    }

    arr = (heap_header**) fn->protos->object.data;
    arr[fn->num_protos] = proto;
    idx = (i32) fn->num_protos;
    fn->num_protos += 1;

    return idx;
}

#define UPVAL_INIT_CAP (2)

i32 mclfunc_add_upval(heap_header* func, char* name, u8 instack, u8 idx,
        MCHeap* heap) {
    mclfunc* fn = func->object.function;
    UpvalDesc* arr = NULL;
    i32 slot = 0;

    if (0 != array_ensure(&fn->upvals, fn->num_upvals,
            sizeof(UpvalDesc), UPVAL_INIT_CAP, heap)) {
        return -1;
    }

    arr = (UpvalDesc*) fn->upvals->object.data;
    arr[fn->num_upvals].name = name;
    arr[fn->num_upvals].instack = instack;
    arr[fn->num_upvals].idx = idx;
    slot = (i32) fn->num_upvals;
    fn->num_upvals += 1;

    return slot;
}

void mclfunc_clear(heap_header* func) {
    func->object.function->code_len = 0;
}
