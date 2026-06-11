#include "mcloadlib.h"
#include <stddef.h>
#include <stdio.h>

#include "mctypes.h"
#include "mclast.h"
#include "mcxt.h"

static i8 tkv_run(TokenVisitor* v, TokenArray* arr) {
    i8 rcode = 0;

    if (NULL != v->init) {
        rcode = v->init(v->ctx, arr);
        if (0 != rcode) {
            return rcode;
        }
    }

    if (NULL != v->visit) {
        for (u64 idx = 0; idx < arr->len; idx += 1) {
            rcode = v->visit(v->ctx, &arr->tkns[idx], idx);
            if (0 != rcode) {
                goto CLEANUP;
            }
        }
    }

CLEANUP:
    if (NULL != v->deinit) {
        i8 dcode = v->deinit(v->ctx);
        if ((0 == rcode) && (0 != dcode)) {
            rcode = dcode;
        }
    }

    return rcode;
}

static i8 astv_walk(ASTVisitor* v, HeapHeader* node, u32 depth) {
    i8 rc = v->visit(v->ctx, node, depth);
    if (0 != rc) {
        return rc;
    }
    HeapHeader** children = mclast_children(node);
    for (u32 idx = 0; idx < AST_NCHILD(node); idx += 1) {
        if (NULL == children[idx]) {
            continue;
        }
        rc = astv_walk(v, children[idx], depth + 1);
        if (0 != rc) {
            return rc;
        }
    }
    return 0;
}

static i8 astv_run(ASTVisitor* v, HeapHeader* root) {
    i8 rcode = 0;

    if (NULL != v->init) {
        rcode = v->init(v->ctx, root);
        if (0 != rcode) {
            return rcode;
        }
    }

    if (NULL != v->visit) {
        rcode = astv_walk(v, root, 0);
    }

    if (NULL != v->deinit) {
        i8 dcode = v->deinit(v->ctx);
        if ((0 == rcode) && (0 != dcode)) {
            rcode = dcode;
        }
    }

    return rcode;
}

void mcxt_init(MCExt* ext, void* mem, u32 max) {
    ext->handles  = (void**) mem;
    ext->visitors = (void**) mem + max;
    ext->tkv_count  = 0;
    ext->astv_count = 0;
    ext->max = max;
}

i8 mcxt_load_tkv(MCExt* ext, const char* path) {
    mctkv_load_fn fn  = NULL;
    u32 slot = ext->tkv_count;

    if (slot >= ext->max) {
        printf("[mcxt] visitor limit reached (max %u)\n", ext->max);
        return -1;
    }

    ext->handles[slot] = LIBOPEN(path);
    if (NULL == ext->handles[slot]) {
        printf("[mcxt] cannot load token visitor %s: %s\n", path, LIBERR());
        return -1;
    }

    *(void**)(&fn) = LIBSYM(ext->handles[slot], "mctkv_load");
    if (NULL == fn) {
        printf("[mcxt] %s: missing symbol mctkv_load\n", path);
        LIBCLOSE(ext->handles[slot]);
        return -1;
    }

    ext->visitors[slot] = fn();
    ext->tkv_count += 1;
    return 0;
}

i8 mcxt_load_astv(MCExt* ext, const char* path) {
    mcastv_load_fn fn = NULL;
    u32 slot = ext->tkv_count + ext->astv_count;

    if (slot >= ext->max) {
        printf("[mcxt] visitor limit reached (max %u)\n", ext->max);
        return -1;
    }

    ext->handles[slot] = LIBOPEN(path);
    if (NULL == ext->handles[slot]) {
        printf("[mcxt] cannot load AST visitor %s: %s\n", path, LIBERR());
        return -1;
    }

    *(void**)(&fn) = LIBSYM(ext->handles[slot], "mcastv_load");
    if (NULL == fn) {
        printf("[mcxt] %s: missing symbol mcastv_load\n", path);
        LIBCLOSE(ext->handles[slot]);
        return -1;
    }

    ext->visitors[slot] = fn();
    ext->astv_count += 1;
    return 0;
}

i8 mcxt_run_tkv(MCExt* ext, TokenArray* arr) {
    i8 rcode = 0;
    for (u32 idx = 0; idx < ext->tkv_count; idx += 1) {
        rcode = tkv_run((TokenVisitor*) ext->visitors[idx], arr);
        if (rcode < 0) {
            printf("[mcxt] token visitor %u failed: %d\n", idx, (int) rcode);
            return rcode;
        }
        rcode = 0;
    }
    return 0;
}

i8 mcxt_run_astv(MCExt* ext, HeapHeader* root) {
    i8 rcode = 0;
    u32 end  = ext->tkv_count + ext->astv_count;

    for (u32 idx = ext->tkv_count; idx < end; idx += 1) {
        rcode = astv_run((ASTVisitor*) ext->visitors[idx], root);
        if (rcode < 0) {
            printf("[mcxt] AST visitor %u failed: %d\n",
                    idx - ext->tkv_count, (int) rcode);
            return rcode;
        }
        rcode = 0;
    }
    return 0;
}

void mcxt_free(MCExt* ext) {
    u32 total = ext->tkv_count + ext->astv_count;

    for (u32 idx = 0; idx < total; idx += 1) {
        LIBCLOSE(ext->handles[idx]);
    }
}
