#include <stdio.h>
#include <string.h>

#include "mcfs.h"
#include "mctypes.h"
#include "mclex.h"

#define MAXFILESIZE (1024)

static void loghelp(void);
static i8 loadscriptfile(str8* fstr, const char* const path);
static i8 interactive(str8* istr);
static void print_tokens(LexState* lstate);

int main(int argc, const char** argv) {
    str8 fbfr;
    i8 isfile = 0;
    LexState lstate;

    if (1 > argc) {
        loghelp();
    }

    for (i32 argIdx = 1; argIdx < argc; argIdx += 1) {
        if (0 == strncmp(argv[argIdx], "-f", 2)) {
            if (argc > (1 + argIdx)) {
                if (0 != loadscriptfile(&fbfr, argv[1 + argIdx])) {
                    return 1;
                }
                isfile = 1;
                argIdx += 1;
            } else {
                printf("No filepath provided\n");
                return -1;
            }
        } else if (0 == strncmp(argv[argIdx], "-i", 2)) {
            interactive(&fbfr);
        } else if ((0 == strncmp(argv[argIdx], "-h", 2))
             || (0 == strncmp(argv[argIdx], "--help", 5))) {
            loghelp();
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[argIdx]);
            loghelp();
            return 1;
        }
    }

    mclex_init(&lstate);
    mclex_lexall(&lstate, &fbfr);
    print_tokens(&lstate);

    mclex_free(&lstate);
    if (1 == isfile) {
        str8_free(&fbfr);
    }

    printf("ova :3\n");
}

static void loghelp(void) {
    printf("[MC] options:\n");
    printf("-f <path> execute passed file\n");
    printf("-i runs dynamic shell\n");
    printf("-h --help show this message\n");
}

static i8 loadscriptfile(str8* fstr, const char* const path) {
    i8 rcode = 0;

    rcode = str8_alloc(fstr, MAXFILESIZE);
    if (0 != rcode) {
        printf("failed to allocate file buffer\n");
        goto LOADSCRIPTEXIT;
    }

    rcode = mcfs_loadtxt(fstr, path);
    if (0 != rcode) {
        printf("failed to read file\n");
        goto LOADSCRIPTEXIT;
    }
    printf("file contents:\n %s\n", fstr->content);

LOADSCRIPTEXIT:
    return rcode;
}

static i8 interactive(str8* istr) {
    printf("interactive? More like hardcoded lol\n");

    char *lua_script =
        "local x = 10\n"
        "function my_cool_lua_function()\n"
        "   return x\n"
        "end\n";

    return str8_attach(istr, lua_script);
}

static void print_tokens(LexState* lstate) {
    TokenArray* tkn_arr = &lstate->token_array;

    for (u64 idx = 0; idx < tkn_arr->len; idx += 1) {
        printf("[%lu]: %ld | %s | ",
               idx,
               tkn_arr->tkns[idx].token_number,
               TK2STR(tkn_arr->tkns[idx].token_number));

        if (tkn_arr->tkns[idx].token_number == TK_NUMBER) {
            printf("%f", tkn_arr->tkns[idx].semantics.number);
        }
        if (tkn_arr->tkns[idx].token_number == TK_NAME) {
            printf("%s", tkn_arr->tkns[idx].semantics.string.content);
        }

        printf("\n");
    }
}
