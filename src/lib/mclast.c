#include <stddef.h>

#include "mctypes.h"

#include "mclast.h"
#include "mcheap.h"

HeapHeader* mclast_alloc(ParseNodeType type, u32 child_cap, MCHeap* heap) {
    HeapHeader* hdr = mcheap_managed_reserve(heap,
            sizeof(mclastnode) + (child_cap * sizeof(HeapHeader*)));
    if (NULL == hdr) {
        return NULL;
    }

    hdr->type = OBJ_AST;
    hdr->object.astnode->type = type;
    hdr->object.astnode->val.integer = 0;
    hdr->object.astnode->child_count = 0;

    return hdr;
}

HeapHeader** mclast_children(HeapHeader* node) {
    return (HeapHeader**)((byte*) node->object.astnode + sizeof(mclastnode));
}

i8 mclast_push_child(HeapHeader* parent, HeapHeader* child, MCHeap* heap) {
    u32 cap = 0;
    u32 newcap = 0;
    u32 reqsize = 0;
    HeapHeader** children = NULL;

    cap = AST_CHILD_CAP(parent);
    if (parent->object.astnode->child_count >= cap) {
        newcap = (0 == cap) ? 4 : (cap * 2);
        reqsize = sizeof(mclastnode) + (newcap * sizeof(HeapHeader*));
        if (0 != mcheap_managed_resize(heap, parent, reqsize)) {
            return -1;
        }
    }

    children = mclast_children(parent);
    children[parent->object.astnode->child_count] = child;
    parent->object.astnode->child_count += 1;

    return 0;
}
