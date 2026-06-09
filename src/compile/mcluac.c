#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mctypes.h"
#include "mcconfig.h"
#include "mcheap.h"
#include "mcstrtbl.h"
#include "mclex.h"
#include "mcparse.h"
#include "mccompile.h"
#include "mclfunc.h"
#include "mclstr.h"
#include "mcfs.h"
#include "mcxt.h"

#define LUAC_VERSION 0x54
#define LUAC_FORMAT  0
#define LUAC_DATA    "\x19\x93\r\n\x1a\n"
#define LUAC_INT     ((i64) 0x5678)
#define LUAC_NUM     ((f64) 370.5)

#define LUA_VNIL     0x00
#define LUA_VFALSE   0x01
#define LUA_VTRUE    0x11
#define LUA_VNUMINT  0x03
#define LUA_VNUMFLT  0x13
#define LUA_VSHRSTR  0x04
#define LUA_VLNGSTR  0x14

#define MAXSHORTLEN 40

typedef struct MCLuac {
    VMConfig    config;
    MCHeap      heap;
    StringTable stringtable;
    LexState    lexstate;
    ParseState  parsestate;
    MCComp      compilestate;
    MCExt       ext;
} MCLuac;

typedef struct LuacBfr {
    mcfile* out;
    byte* bfr;
    u64 cap;
    u64 len;
} LuacBfr;

static i8 luac_flush(LuacBfr* bfr);
static i8 luac_put(LuacBfr* bfr, const void* data, u64 n);
static i8 luac_dump_size(LuacBfr* bfr, u64 x);
static i8 luac_dump_string(LuacBfr* bfr, const char* str, u64 len);
static i8 luac_dump_header(LuacBfr* bfr, u8 main_upvals);
static i8 luac_dump_constants(LuacBfr* bfr, heap_header* func);
static i8 luac_dump_upvalues(LuacBfr* bfr, heap_header* func);
static i8 luac_dump_debug(LuacBfr* bfr);
static i8 luac_dump_function(LuacBfr* bfr, heap_header* func, u8 is_main,
        const char* source);
static i8 luac_tofile(heap_header* func, const char* path,
        const char* chunkname, byte* scratch, u64 scratchcap);
static i8 luac_init(MCLuac* mc);
static void luac_free(MCLuac* mc);
static i8 luac_compile_file(MCLuac* mc, const char* in, const char* out);
static void luac_loghelp(void);

static i8 luac_flush(LuacBfr* bfr) {
    i8 rcode = 0;
    if (bfr->len > 0) {
        rcode = mcfs_writebytes(bfr->out, bfr->bfr, bfr->len);
        bfr->len = 0;
    }
    return rcode;
}

static i8 luac_put(LuacBfr* bfr, const void* data, u64 n) {
    if (n > bfr->cap) {
        if (0 != luac_flush(bfr)) {
            return -1;
        }
        return mcfs_writebytes(bfr->out, data, n);
    }

    if ((bfr->len + n) > bfr->cap) {
        if (0 != luac_flush(bfr)) {
            return -1;
        }
    }

    memcpy(bfr->bfr + bfr->len, data, n);
    bfr->len += n;
    return 0;
}

static i8 luac_dump_size(LuacBfr* bfr, u64 x) {
    u8 buf[16];
    i32 n = 0;

    do {
        n += 1;
        buf[sizeof(buf) - n] = (u8) (x & 0x7f);
        x >>= 7;
    } while (0 != x);

    buf[sizeof(buf) - 1] |= 0x80;

    return luac_put(bfr, buf + sizeof(buf) - n, (u64) n);
}

static i8 luac_dump_string(LuacBfr* bfr, const char* str, u64 len) {
    if (NULL == str) {
        return luac_dump_size(bfr, 0);
    }

    if (0 != luac_dump_size(bfr, len + 1)) {
        return -1;
    }

    return luac_put(bfr, str, len);
}

static i8 luac_dump_header(LuacBfr* bfr, u8 main_upvals) {
    byte hdr[33];
    u64 p = 0;
    i64 luac_int = LUAC_INT;
    f64 luac_num = LUAC_NUM;

    memcpy(hdr + p, "\x1bLua", 4);
    p += 4;

    hdr[p++] = LUAC_VERSION;
    hdr[p++] = LUAC_FORMAT;
    memcpy(hdr + p, LUAC_DATA, sizeof(LUAC_DATA) - 1);
    p += sizeof(LUAC_DATA) - 1;

    hdr[p++] = (u8) sizeof(u32);
    hdr[p++] = (u8) sizeof(i64);
    hdr[p++] = (u8) sizeof(f64);
    memcpy(hdr + p, &luac_int, sizeof(luac_int));
    p += sizeof(luac_int);

    memcpy(hdr + p, &luac_num, sizeof(luac_num));
    p += sizeof(luac_num);
    hdr[p++] = main_upvals;

    return luac_put(bfr, hdr, p);
}

static i8 luac_dump_constants(LuacBfr* bfr, heap_header* func) {
    mclfunc* fn = func->object.function;
    Constant* consts = mclfunc_getconsts(func);
    u32 idx = 0;
    heap_header* str = NULL;
    u32 slen = 0;

    if (0 != luac_dump_size(bfr, fn->num_consts)) {
        return -1;
    }
    for (idx = 0; idx < fn->num_consts; idx += 1) {
        switch (consts[idx].type) {
            case K_INT:
                if (0 != luac_put(bfr, &(u8){LUA_VNUMINT}, 1)) {
                    return -1;
                }
                if (0 != luac_put(bfr, &consts[idx].val.integer,
                        sizeof(consts[idx].val.integer))) {
                    return -1;
                }
                break;
            case K_FLT:
                if (0 != luac_put(bfr, &(u8){LUA_VNUMFLT}, 1)) {
                    return -1;
                }
                if (0 != luac_put(bfr, &consts[idx].val.number,
                        sizeof(consts[idx].val.number))) {
                    return -1;
                }
                break;
            case K_STR:
                str = consts[idx].val.heapobj;
                slen = str->object.string->len;
                if (0 != luac_put(bfr, &(u8){
                        (slen <= MAXSHORTLEN) ? LUA_VSHRSTR : LUA_VLNGSTR}, 1)) {
                    return -1;
                }
                if (0 != luac_dump_string(bfr, mclstr_getchars(str), slen)) {
                    return -1;
                }
                break;
        }
    }
    return 0;
}

static i8 luac_dump_upvalues(LuacBfr* bfr, heap_header* func) {
    mclfunc* fn = func->object.function;
    UpvalDesc* ups = mclfunc_getupvals(func);
    u32 idx = 0;

    if (0 != luac_dump_size(bfr, fn->num_upvals)) {
        return -1;
    }
    for (idx = 0; idx < fn->num_upvals; idx += 1) {
        if (0 != luac_put(bfr, &(u8){ups[idx].instack}, 1)) {
            return -1;
        }
        if (0 != luac_put(bfr, &(u8){ups[idx].idx}, 1)) {
            return -1;
        }
        if (0 != luac_put(bfr, &(u8){0}, 1)) {       
            return -1;
        }
    }
    return 0;
}



static i8 luac_dump_debug(LuacBfr* bfr) {
    if (0 != luac_dump_size(bfr, 0)) {
        return -1;
    }
    if (0 != luac_dump_size(bfr, 0)) {
        return -1;
    }
    if (0 != luac_dump_size(bfr, 0)) {
        return -1;
    }
    return luac_dump_size(bfr, 0);
}

static i8 luac_dump_function(LuacBfr* bfr, heap_header* func, u8 is_main,
        const char* source) {
    mclfunc* fn = func->object.function;
    u32* code = mclfunc_getbody(func);
    heap_header** protos = mclfunc_getprotos(func);
    u32 idx = 0;

    if (0 != luac_dump_string(bfr, is_main ? source : NULL,
            is_main ? strlen(source) : 0)) {
        return -1;
    }
    if (0 != luac_dump_size(bfr, 0)) {        
        return -1;
    }
    if (0 != luac_dump_size(bfr, 0)) {        
        return -1;
    }
    if (0 != luac_put(bfr, &(u8){fn->num_params}, 1)) {
        return -1;
    }
    if (0 != luac_put(bfr, &(u8){fn->is_vararg}, 1)) {
        return -1;
    }
    if (0 != luac_put(bfr, &(u8){fn->max_stack}, 1)) {
        return -1;
    }

    if (0 != luac_dump_size(bfr, fn->code_len)) {
        return -1;
    }
    if (0 != luac_put(bfr, code, (u64) fn->code_len * sizeof(u32))) {
        return -1;
    }

    if (0 != luac_dump_constants(bfr, func)) {
        return -1;
    }
    if (0 != luac_dump_upvalues(bfr, func)) {
        return -1;
    }

    if (0 != luac_dump_size(bfr, fn->num_protos)) {
        return -1;
    }
    for (idx = 0; idx < fn->num_protos; idx += 1) {
        if (0 != luac_dump_function(bfr, protos[idx], 0, source)) {
            return -1;
        }
    }

    return luac_dump_debug(bfr);
}

static i8 luac_tofile(heap_header* func, const char* path,
        const char* chunkname, byte* scratch, u64 scratchcap) {
    mcfile out;
    LuacBfr b;
    i8 rcode = 0;

    if ((NULL == func) || (OBJ_FUNC != func->type)) {
        printf("[mcluac] not a function object\n");
        return -1;
    }

    if (0 != mcfs_openfile(&out, path, "wb")) {
        printf("[mcluac] cannot open output file: %s\n", path);
        return -1;
    }

    b.out = &out;
    b.bfr = scratch;
    b.cap = scratchcap;
    b.len = 0;

    if (0 != luac_dump_header(&b, func->object.function->num_upvals)) {
        rcode = -1;
        goto DUMPEXIT;
    }
    rcode = luac_dump_function(&b, func, 1, chunkname);
    if (0 == rcode) {
        rcode = luac_flush(&b);     
    }

DUMPEXIT:
    mcfs_closefile(&out);
    if (0 != rcode) {
        printf("[mcluac] failed writing %s\n", path);
    }
    return rcode;
}

static i8 luac_init(MCLuac* mc) {
    i8 rcode = 0;
    VMConfig* cfg = &mc->config;

    mcvm_set_default_config(cfg);

    rcode = mcheap_init(&mc->heap, cfg->MAX_MEM);
    if (0 != rcode) {
        printf("[mcluac] failed to initialize heap\n");
        return -1;
    }

    mcxt_init(&mc->ext,
            mcheap_static_reserve(&mc->heap,
                    (u64) cfg->MAX_EXT_VISITORS * 2 * sizeof(void*)),
            cfg->MAX_EXT_VISITORS);

    rcode = mcstrtbl_init(&mc->stringtable,
            cfg->MAX_VARNAME_ENTRIES,
            mcheap_static_reserve(&mc->heap, cfg->MAX_VARNAME_POOL_SIZE),
            cfg->MAX_VARNAME_POOL_SIZE);
    if (0 != rcode) {
        printf("[mcluac] failed to initialize string table\n");
        goto INITFAIL;
    }

    mc->lexstate.stringtable = &mc->stringtable;
    mc->lexstate.heap = &mc->heap;
    rcode = mclex_init(&mc->lexstate, cfg,
            mcheap_static_reserve(&mc->heap, cfg->MAX_LEX_MEM),
            cfg->MAX_LEX_MEM);
    if (0 != rcode) {
        printf("[mcluac] failed to initialize lexer\n");
        goto INITFAIL;
    }

    rcode = mcparse_init(&mc->parsestate, &mc->heap);
    if (0 != rcode) {
        printf("[mcluac] failed to initialize parser\n");
        goto INITFAIL;
    }

    rcode = mccomp_init(&mc->compilestate, &mc->heap, cfg, &mc->stringtable);
    if (0 != rcode) {
        printf("[mcluac] failed to initialize compiler\n");
        goto INITFAIL;
    }

    return 0;
INITFAIL:
    mcheap_free(&mc->heap);
    return -1;
}

static void luac_free(MCLuac* mc) {
    mcxt_free(&mc->ext);
    mcheap_free(&mc->heap);
}

static i8 luac_compile_file(MCLuac* mc, const char* in, const char* out) {
    i8 rcode = 0;
    heap_header* root = NULL;
    heap_header* func = NULL;

    rcode = mclex_lexscript_file(&mc->lexstate, in);
    if (0 != rcode) {
        printf("[mcluac] failed to lex %s\n", in);
        return -1;
    }

    rcode = mcxt_run_tkv(&mc->ext, &mc->lexstate.token_array);
    if (0 != rcode) {
        return rcode;
    }

    rcode = mcparse_run(&mc->parsestate, &mc->lexstate.token_array);
    if (0 != rcode) {
        printf("[mcluac] failed to parse %s\n", in);
        return -1;
    }
    root = mc->parsestate.root;

    rcode = mcxt_run_astv(&mc->ext, root);
    if (0 != rcode) {
        return rcode;
    }

    func = mccomp_run(&mc->compilestate, root);
    if (NULL == func) {
        printf("[mcluac] failed to compile %s\n", in);
        return -1;
    }
    if (NULL != getenv("MCLUAC_DEBUG")) {
        mccomp_log(func);
    }
    
    return luac_tofile(func, out, in, (byte*) mc->lexstate.input,
            mc->config.MAX_FILE_CHUNK_SIZE);
}

int main(int argc, const char** argv) {
    MCLuac      mc;
    const char* inpath  = NULL;
    const char* outpath = NULL;
    const char* tkv_paths[32];
    const char* astv_paths[32];
    u32  tkv_n  = 0;
    u32  astv_n = 0;
    u32  i      = 0;
    i32  idx    = 0;
    i8   rcode  = 0;

    memset(&mc, 0, sizeof(mc));

    for (idx = 1; idx < argc; idx += 1) {
        if (0 == strncmp(argv[idx], "-i", 2)) {
            if (argc <= (idx + 1)) {
                printf("[mcluac] -i needs a path\n");
                return 1;
            }
            inpath = argv[idx + 1];
            idx += 1;
        } else if (0 == strncmp(argv[idx], "-o", 2)) {
            if (argc <= (idx + 1)) {
                printf("[mcluac] -o needs a path\n");
                return 1;
            }
            outpath = argv[idx + 1];
            idx += 1;
        } else if (0 == strncmp(argv[idx], "-xt", 3)) {
            if (argc <= (idx + 1)) {
                printf("[mcluac] -xt needs a path\n");
                return 1;
            }
            if (tkv_n >= 32) {
                printf("[mcluac] too many -xt flags\n");
                return 1;
            }
            tkv_paths[tkv_n] = argv[idx + 1];
            tkv_n += 1;
            idx += 1;
        } else if (0 == strncmp(argv[idx], "-xa", 3)) {
            if (argc <= (idx + 1)) {
                printf("[mcluac] -xa needs a path\n");
                return 1;
            }
            if (astv_n >= 32) {
                printf("[mcluac] too many -xa flags\n");
                return 1;
            }
            astv_paths[astv_n] = argv[idx + 1];
            astv_n += 1;
            idx += 1;
        } else if ((0 == strncmp(argv[idx], "-h", 2))
                || (0 == strncmp(argv[idx], "--help", 6))) {
            luac_loghelp();
            return 0;
        } else {
            printf("[mcluac] unknown option: %s\n", argv[idx]);
            luac_loghelp();
            return 1;
        }
    }

    if ((NULL == inpath) || (NULL == outpath)) {
        luac_loghelp();
        return 1;
    }

    if (0 != luac_init(&mc)) {
        return 1;
    }

    for (i = 0; i < tkv_n; i += 1) {
        if (0 != mcxt_load_tkv(&mc.ext, tkv_paths[i])) {
            luac_free(&mc);
            return 1;
        }
    }
    for (i = 0; i < astv_n; i += 1) {
        if (0 != mcxt_load_astv(&mc.ext, astv_paths[i])) {
            luac_free(&mc);
            return 1;
        }
    }

    rcode = luac_compile_file(&mc, inpath, outpath);
    if (0 == rcode) {
        printf("[mcluac] wrote %s\n", outpath);
    }

    luac_free(&mc);
    return rcode;
}

static void luac_loghelp(void) {
    printf("[mcluac] Lua source to 5.4 bytecode compiler\n"
           "usage: mcluac -i <script> -o <out> [-xt <lib>]... [-xa <lib>]...\n"
           "  -i <path>   input Lua script\n"
           "  -o <path>   output bytecode chunk\n"
           "  -xt <path>  token visitor shared library\n"
           "  -xa <path>  AST visitor shared library\n"
           "  -h --help   show this message\n");
}
