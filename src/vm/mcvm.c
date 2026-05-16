#include <stdlib.h>

#include "mcfs.h"
#include "mcstr.h"
#include "mctypes.h"

#include "mcvm.h"

i8 mcvm_init(VMState* vm, VMConfig* cfg) {
    i8 initcode = 0;

    if (NULL == cfg) {
        cfg = &vm->config;
        mcvm_set_default_config(cfg);
    } else {
        initcode = mcvm_validate_config(cfg);
        if (0 != initcode) {
            printf("invalid config\n");
            goto VMINITEXIT;
        }
    }

    u64 scratch_size = cfg->MAX_IDLEN;
    u64 lex_size = (cfg->MAX_TOKENS * sizeof(Token)) + scratch_size;
    u64 strtbl_size = cfg->MAX_VARNAME_POOL_SIZE;
    u64 total_size = lex_size + strtbl_size;

    char* mem = (char*) malloc(total_size);
    if (NULL == mem) {
        printf("OOMed lol\n");
        initcode = -1;
        goto VMINITEXIT;
    }
    vm->mem = mem;

    initcode = mclex_init(&vm->lexstate, mem, lex_size);
    if (0 != initcode) {
        printf("failed to initialize lexer\n");
        goto VMINITEXIT_POSTALLOC;
    }
    vm->lexstate.config = &vm->config;
    vm->lexstate.stringtable = &vm->stringtable;

    initcode = mcstrtbl_init(&vm->stringtable,
            mem + lex_size,
            strtbl_size,
            cfg->MAX_VARNAME_ENTRIES);
    if (0 != initcode) {
        printf("failed to initialize string table\n");
        goto VMINITEXIT_POSTALLOC;
    }

VMINITEXIT_POSTALLOC:
    free(mem);
VMINITEXIT:
    return initcode;
}

void mcvm_free(VMState* vm) {
    free(vm->mem);
}

i8 mcvm_parsestr8(VMState* vm, str8* str) {
    i8 rcode = 0;

    rcode = mclex_lexscript_str8(&vm->lexstate, str);
    mclex_logtokens(&vm->lexstate);

    return rcode;
}

i8 mcvm_parsestr0(VMState* vm, const char* str) {
    i8 rcode = 0;
    str8 temp_str;

    rcode = str8_attach_nt(&temp_str, str);
    if (0 != rcode) {
        printf("failed to reserve string buffer\n");
        goto PARSESTR0EXIT;
    }

    rcode = mcvm_parsestr8(vm, &temp_str);

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

    rcode = mcvm_parsestr8(vm, &file_buffer);

PARSESCRIPTEXIT:
    return rcode;
}
