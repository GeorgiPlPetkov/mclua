#include <stdlib.h>

#include "mcfs.h"
#include "mcstr.h"
#include "mctypes.h"

#include "mcvm.h"

i8 mcvm_init(VMState* vm, VMConfig* cfg) {
    i8 initcode = 0;

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

    initcode = mcheap_init(&vm->heap, cfg->MAX_MEM);
    if (0 != initcode) {
        printf("failed to initialize heap\n");
        initcode = -1;
        goto VMINITEXIT_PREALLOC;
    }

    initcode = mcstrtbl_init(&vm->stringtable,
            cfg->MAX_VARNAME_ENTRIES,
            mcheap_static_reserve(&vm->heap, cfg->MAX_VARNAME_POOL_SIZE),
            cfg->MAX_VARNAME_POOL_SIZE);
    if (0 != initcode) {
        printf("failed to initialize string table\n");
        goto VMINITEXIT_POSTALLOC;
    }

    vm->lexstate.stringtable = &vm->stringtable;
    vm->lexstate.heap = &vm->heap;
    initcode = mclex_init(&vm->lexstate,
            &vm->config,
            mcheap_static_reserve(&vm->heap, cfg->MAX_LEX_MEM),
            cfg->MAX_LEX_MEM);
    if (0 != initcode) {
        printf("failed to initialize lexer\n");
        goto VMINITEXIT_POSTALLOC;
    }

    initcode = mcparse_init(&vm->parsestate, &vm->heap);
    if (0 != initcode) {
        printf("failed to initialize parser\n");
        goto VMINITEXIT_POSTALLOC;
    }

    return 0;
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
    if (0 != rcode) {
        goto LEAVEPARSE;
    }

    rcode = mcparse_run(&vm->parsestate, &vm->lexstate.token_array);
    if (0 != rcode) {
        printf("failed to parse\n");
        goto LEAVEPARSE;
    }
    mcparse_log_node(vm->parsestate.root, 0);

LEAVEPARSE:
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

    rcode = mclex_lexscript_file(&vm->lexstate, path);
    if (0 != rcode) {
        printf("failed at lexer\n");
        goto PARSESCRIPTEXIT;
    }
    mclex_logtokens(&vm->lexstate);

    rcode = mcparse_run(&vm->parsestate, &vm->lexstate.token_array);
    if (0 != rcode) {
        printf("failed at parser\n");
        goto PARSESCRIPTEXIT;
    }
    mcparse_log_node(vm->parsestate.root, 0);

PARSESCRIPTEXIT:
    return rcode;
}
