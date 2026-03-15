#include <stdlib.h>

#include "mcstr.h"

i8 str8_alloc(str8* str, u64 cap) {
    str->content = (char*) calloc(cap, 1);

    if (NULL == str->content) {
        return -1;
    }

    str->capacity = cap;
    str->lenght = 0;

    return 0;
}

void str8_free(str8* str) {
    free(str->content);
    str->capacity = 0;
    str->lenght = 0;
}

i8 str8_attach(str8* str, char* bfr) {
    if (NULL == bfr) {
        return -1;
    }

    str->content = bfr;
    str->capacity = strlen(bfr);
    str->lenght = str->capacity;

    return 0;
}
