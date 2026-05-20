#include <stdlib.h>

#include "mcfs.h"
#include "mcstr.h"
#include "mctypes.h"

#include "mcvm.h"

i8 mcvm_init(VMState* vm, VMConfig* cfg) {
    i8 initcode = 0;
    u64 lexmem_size = 0;
    u64 strtblmem_size = 0;
    u64 totalmem = 0;

    if (NULL == cfg) {
        printf("no config provided, using default\n");
        cfg = &vm->config;
        mcvm_set_default_config(cfg);
    } else {
        initcode = mcvm_validate_config(cfg);
        if (0 != initcode) {
            printf("invalid config\n");
            goto VMINITEXIT_PREALLOC;
        }
    }

    lexmem_size = cfg->MAX_LEX_MEM;
    strtblmem_size = cfg->MAX_VARNAME_POOL_SIZE;
    totalmem = lexmem_size + strtblmem_size;

    initcode = mcheap_init(&vm->heap, totalmem);
    if (0 != initcode) {
        printf("failed to initialize heap\n");
        initcode = -1;
        goto VMINITEXIT_PREALLOC;
    }

    initcode = mcstrtbl_init(&vm->stringtable,
            cfg->MAX_VARNAME_ENTRIES,
            mcheap_reserve(&vm->heap, strtblmem_size),
            strtblmem_size);
    if (0 != initcode) {
        printf("failed to initialize string table\n");
        goto VMINITEXIT_POSTALLOC;
    }

    vm->lexstate.config = &vm->config;
    vm->lexstate.stringtable = &vm->stringtable;
    initcode = mclex_init(&vm->lexstate,
            mcheap_reserve(&vm->heap, lexmem_size),
            lexmem_size);
    if (0 != initcode) {
        printf("failed to initialize lexer\n");
        goto VMINITEXIT_POSTALLOC;
    }

VMINITEXIT_POSTALLOC:
    mcheap_free(&vm->heap);
VMINITEXIT_PREALLOC:
    return initcode;
}

void mcvm_free(VMState* vm) {
    mcheap_free(&vm->heap);
}

i8 mcvm_parse_str8(VMState* vm, str8* str) {
    i8 rcode = 0;

    rcode = mclex_lexscript_str8(&vm->lexstate, str);
    mclex_logtokens(&vm->lexstate);

    return rcode;
}

i8 mcvm_parse_str0(VMState* vm, const char* str) {
    i8 rcode = 0;
    str8 temp_str;

    rcode = str8_attach_nt(&temp_str, str);
    if (0 != rcode) {
        printf("failed to reserve string buffer\n");
        goto PARSESTR0EXIT;
    }

    rcode = mcvm_parse_str8(vm, &temp_str);

PARSESTR0EXIT:
    return rcode;
}

i8 mcvm_parsefile(VMState* vm, const char* path) {
    i8 rcode = 0;
    char bfr[vm->config.MAX_FILE_SIZE];
    str8 file_buffer;

    rcode = str8_attach_nt(&file_buffer, bfr);
    if (0 != rcode) {
        printf("failed to reserve file buffer\n");
        goto PARSESCRIPTEXIT;
    }

    rcode = mcfs_loadas_str8(&file_buffer, path);
    if (0 != rcode) {
        printf("failed to read file\n");
        goto PARSESCRIPTEXIT;
    }

    rcode = mcvm_parse_str8(vm, &file_buffer);

PARSESCRIPTEXIT:
    return rcode;
}
