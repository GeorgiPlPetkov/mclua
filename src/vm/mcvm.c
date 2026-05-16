#include <stdlib.h>

#include "mcfs.h"
#include "mcstr.h"
#include "mctypes.h"

#include "mcvm.h"

#define MAXFILESIZE (1024)
#define MAX_VARNAMES (1024)
#define MAX_VARMEM (MAX_VARNAMES * 32)

i8 mcvm_init(VMState* vm, VMConfig* cfg) {
    i8 initcode = 0;
    (void) cfg;

    char* strtbl_bfr = (char*) malloc(MAX_VARMEM);
    if (NULL == strtbl_bfr) {
        printf("OOMed on the string table oof\n");
        goto VMINITEXIT;
    }
    initcode = mcstrtbl_init(&vm->stringtable,
            strtbl_bfr,
            MAX_VARMEM,
            MAX_VARNAMES);
    if (0 != initcode) {
        printf("failed to initialize string table\n");
        goto VMINITEXIT;
    }
    initcode = mclex_init(&vm->lexstate);
    if (0 != initcode) {
        printf("failed to initialize lexer\n");
    }
    vm->lexstate.stringtable = &vm->stringtable;

VMINITEXIT:
    return initcode;
}

void mcvm_free(VMState* vm) {
    mclex_free(&vm->lexstate);
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
    char bfr[MAXFILESIZE];
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
