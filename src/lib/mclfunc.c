#include <string.h>

#include "mctypes.h"
#include "mcmemops.h"

#include "mclfunc.h"
#include "mclstr.h"
#include "mcheap.h"

#define CONST_INIT_CAP (4)
#define PROTO_INIT_CAP (2)
#define CONST_INDEX_INIT_CAP (16)

typedef struct ConstIndexEntry {
    u32 hash;
    i32 idx;
} ConstIndexEntry;

static u32 const_hash_int(i64 value) {
    u64 h = (u64) value;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return (u32) h;
}

static u32 const_hash_flt(f64 value) {
    u64 bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return const_hash_int((i64) bits);
}

static u32 const_hash_str(HeapHeader* str) {
    char* chars = mclstr_getchars(str);
    u32 len = str->object.string->len;
    u32 hash = 2166136261u;
    u32 idx = 0;

    for (idx = 0; idx < len; idx += 1) {
        hash ^= (uchar) chars[idx];
        hash *= 16777619u;
    }
    return hash;
}

static ConstIndexEntry* const_index_entries(HeapHeader* func) {
    HeapHeader* idxobj = func->object.function->const_index;
    if (NULL == idxobj) {
        return NULL;
    }
    return (ConstIndexEntry*) idxobj->object.data;
}

static void const_index_insert_raw(ConstIndexEntry* entries, u32 cap,
        u32 hash, i32 idx) {
    u32 slot = hash % cap;

    while (-1 != entries[slot].idx) {
        slot = (slot + 1) % cap;
    }
    entries[slot].hash = hash;
    entries[slot].idx = idx;
}

static i8 const_index_grow(HeapHeader* func, MCHeap* heap) {
    mclfunc* fn = func->object.function;
    ConstIndexEntry* old = const_index_entries(func);
    u32 oldcap = fn->const_index_cap;
    u32 newcap = oldcap * 2;
    HeapHeader* newhdr = mcheap_managed_reserve(heap,
            newcap * sizeof(ConstIndexEntry));
    ConstIndexEntry* newentries = NULL;
    u32 idx = 0;

    if (NULL == newhdr) {
        return -1;
    }
    newhdr->type = OBJ_USR;
    newentries = (ConstIndexEntry*) newhdr->object.data;
    memset(newentries, 0xFF, newcap * sizeof(ConstIndexEntry));

    for (idx = 0; idx < oldcap; idx += 1) {
        if (-1 != old[idx].idx) {
            const_index_insert_raw(newentries, newcap, old[idx].hash, old[idx].idx);
        }
    }

    fn->const_index = newhdr;
    fn->const_index_cap = newcap;
    return 0;
}

static i8 const_index_ensure(HeapHeader* func, MCHeap* heap) {
    mclfunc* fn = func->object.function;
    ConstIndexEntry* entries = NULL;

    if (NULL == fn->const_index) {
        fn->const_index = mcheap_managed_reserve(heap,
                CONST_INDEX_INIT_CAP * sizeof(ConstIndexEntry));
        if (NULL == fn->const_index) {
            return -1;
        }
        fn->const_index->type = OBJ_USR;
        fn->const_index_cap = CONST_INDEX_INIT_CAP;

        entries = (ConstIndexEntry*) fn->const_index->object.data;
        memset(entries, 0xFF, CONST_INDEX_INIT_CAP * sizeof(ConstIndexEntry));
        return 0;
    }

    if (((fn->num_consts + 1) * 10) >= (fn->const_index_cap * 7)) {
        return const_index_grow(func, heap);
    }

    return 0;
}

HeapHeader* mclfunc_alloc(u32 bodycap, MCHeap* heap) {
    HeapHeader* hdr = mcheap_managed_reserve(heap,
            sizeof(mclfunc) + (bodycap * sizeof(u32)));
    if (NULL == hdr) {
        return NULL;
    }

    hdr->type = OBJ_FUNC;
    MemsetZeroStruct(hdr->object.function);

    return hdr;
}

u32* mclfunc_getbody(HeapHeader* func) {
    return (u32*)((byte*) func->object.function + sizeof(mclfunc));
}

i8 mclfunc_append_instr(HeapHeader* func, u32 instr, MCHeap* heap) {
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

Constant* mclfunc_getconsts(HeapHeader* func) {
    if (NULL == func->object.function->constants) {
        return NULL;
    }
    return (Constant*) func->object.function->constants->object.data;
}

HeapHeader** mclfunc_getprotos(HeapHeader* func) {
    if (NULL == func->object.function->protos) {
        return NULL;
    }
    return (HeapHeader**) func->object.function->protos->object.data;
}

UpvalDesc* mclfunc_getupvals(HeapHeader* func) {
    if (NULL == func->object.function->upvals) {
        return NULL;
    }
    return (UpvalDesc*) func->object.function->upvals->object.data;
}



static i8 array_ensure(HeapHeader** slot, u32 count, u32 elemsize,
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

static i32 mclfunc_push_const(HeapHeader* func, Constant value, MCHeap* heap) {
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

i32 mclfunc_const_int(HeapHeader* func, i64 value, MCHeap* heap) {
    Constant entry = {0, {0}};
    ConstIndexEntry* entries = NULL;
    Constant* arr = NULL;
    u32 hash = const_hash_int(value);
    u32 cap = 0;
    u32 slot = 0;
    i32 found = 0;
    i32 newidx = 0;

    if (0 != const_index_ensure(func, heap)) {
        return -1;
    }

    arr = mclfunc_getconsts(func);
    entries = const_index_entries(func);
    cap = func->object.function->const_index_cap;
    slot = hash % cap;

    while (-1 != entries[slot].idx) {
        found = entries[slot].idx;
        if ((entries[slot].hash == hash)
                && (K_INT == arr[found].type)
                && (value == arr[found].val.integer)) {
            return found;
        }
        slot = (slot + 1) % cap;
    }

    entry.type = K_INT;
    entry.val.integer = value;
    newidx = mclfunc_push_const(func, entry, heap);
    if (newidx < 0) {
        return -1;
    }
    const_index_insert_raw(entries, cap, hash, newidx);
    return newidx;
}

i32 mclfunc_const_flt(HeapHeader* func, f64 value, MCHeap* heap) {
    Constant entry = {0, {0}};
    ConstIndexEntry* entries = NULL;
    Constant* arr = NULL;
    u32 hash = const_hash_flt(value);
    u32 cap = 0;
    u32 slot = 0;
    i32 found = 0;
    i32 newidx = 0;

    if (0 != const_index_ensure(func, heap)) {
        return -1;
    }

    arr = mclfunc_getconsts(func);
    entries = const_index_entries(func);
    cap = func->object.function->const_index_cap;
    slot = hash % cap;

    while (-1 != entries[slot].idx) {
        found = entries[slot].idx;
        if ((entries[slot].hash == hash)
                && (K_FLT == arr[found].type)
                && (value == arr[found].val.number)) {
            return found;
        }
        slot = (slot + 1) % cap;
    }

    entry.type = K_FLT;
    entry.val.number = value;
    newidx = mclfunc_push_const(func, entry, heap);
    if (newidx < 0) {
        return -1;
    }
    const_index_insert_raw(entries, cap, hash, newidx);
    return newidx;
}

i32 mclfunc_const_str(HeapHeader* func, HeapHeader* str, MCHeap* heap) {
    Constant entry = {0, {0}};
    ConstIndexEntry* entries = NULL;
    Constant* arr = NULL;
    u32 hash = const_hash_str(str);
    u32 cap = 0;
    u32 slot = 0;
    i32 found = 0;
    i32 newidx = 0;

    if (0 != const_index_ensure(func, heap)) {
        return -1;
    }

    arr = mclfunc_getconsts(func);
    entries = const_index_entries(func);
    cap = func->object.function->const_index_cap;
    slot = hash % cap;

    while (-1 != entries[slot].idx) {
        found = entries[slot].idx;
        if ((entries[slot].hash == hash)
                && (K_STR == arr[found].type)
                && (0 == mclstr_cmp(str, arr[found].val.heapobj))) {
            return found;
        }
        slot = (slot + 1) % cap;
    }

    entry.type = K_STR;
    entry.val.heapobj = str;
    newidx = mclfunc_push_const(func, entry, heap);
    if (newidx < 0) {
        return -1;
    }
    const_index_insert_raw(entries, cap, hash, newidx);
    return newidx;
}

i32 mclfunc_add_proto(HeapHeader* func, HeapHeader* proto, MCHeap* heap) {
    mclfunc* fn = func->object.function;
    HeapHeader** arr = NULL;
    i32 idx = 0;

    if (0 != array_ensure(&fn->protos, fn->num_protos,
            sizeof(HeapHeader*), PROTO_INIT_CAP, heap)) {
        return -1;
    }

    arr = (HeapHeader**) fn->protos->object.data;
    arr[fn->num_protos] = proto;
    idx = (i32) fn->num_protos;
    fn->num_protos += 1;

    return idx;
}

#define UPVAL_INIT_CAP (2)

i32 mclfunc_add_upval(HeapHeader* func, char* name, u8 instack, u8 idx,
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

void mclfunc_clear(HeapHeader* func) {
    func->object.function->code_len = 0;
}
