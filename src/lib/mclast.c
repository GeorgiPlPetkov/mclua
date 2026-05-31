#include <stddef.h>

#include "mctypes.h"

#include "mclast.h"
#include "mcheap.h"

heap_header* mclast_alloc(ParseNodeType type, u32 child_cap, MCHeap* heap) {
    heap_header* hdr = mcheap_managed_reserve(heap,
            sizeof(mclastnode) + (child_cap * sizeof(heap_header*)));
    if (NULL == hdr) {
        return NULL;
    }

    hdr->type = OBJ_AST;
    hdr->object.astnode->type = type;
    hdr->object.astnode->val.integer = 0;
    hdr->object.astnode->child_count = 0;

    return hdr;
}

heap_header** mclast_children(heap_header* node) {
    return (heap_header**)((byte*) node->object.astnode + sizeof(mclastnode));
}

i8 mclast_push_child(heap_header* parent, heap_header* child, MCHeap* heap) {
    u32 cap = 0;
    u32 newcap = 0;
    u32 reqsize = 0;
    heap_header** children = NULL;

    cap = AST_CHILD_CAP(parent);
    if (parent->object.astnode->child_count >= cap) {
        newcap = (0 == cap) ? 4 : (cap * 2);
        reqsize = sizeof(mclastnode) + (newcap * sizeof(heap_header*));
        if (0 != mcheap_managed_resize(heap, parent, reqsize)) {
            return -1;
        }
    }

    children = mclast_children(parent);
    children[parent->object.astnode->child_count] = child;
    parent->object.astnode->child_count += 1;

    return 0;
}
