#include <stdio.h>
#include <string.h>

#include "mcfs.h"
#include "mctypes.h"

#include "mcvm.h"

static void loghelp(void);
static const char* lua_script =
    "local x = 67\n"
    "local y = 0x1A4\n"
    "function my_cool_lua_function()\n"
    "    return (x + y)\n"
    "end\n"
    "print(\"result=\"..my_cool_lua_function())\n";

int main(int argc, const char** argv) {
    int rcode = 0;
    VMState virtman;
    // VMConfig cfg = {
    //     .MAX_MEM = 1024 * 1024,
    //     .MAX_FILE_SIZE = 1024 * 1024
    // };
    mcvm_init(&virtman, NULL);
    printf("[MC] options:\n");
    if (1 > argc) {
        loghelp();
        goto LEAVE;
    }

    for (i32 argIdx = 1; argIdx < argc; argIdx += 1) {
        if (0 == strncmp(argv[argIdx], "-f", 2)) {
            if (argc > (1 + argIdx)) {
                rcode = mcvm_parsefile(&virtman, argv[1 + argIdx]);
                argIdx += 1;
            } else {
                printf("No filepath provided\n");
                return -1;
            }
        } else if (0 == strncmp(argv[argIdx], "-i", 2)) {
            printf("interactive? More like hardcoded lol\n");
            mcvm_parsestr0(&virtman, lua_script);
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

LEAVE:
    mcvm_free(&virtman);
    printf("ova :3\n");

    return rcode;
}

static void loghelp(void) {
    printf("[MC] options:\n");
    printf("-f <path> execute passed file\n");
    printf("-i runs dynamic shell\n");
    printf("-h --help show this message\n");
}
