#include <stdlib.h>

#include "mcstr.h"

i8 str8_alloc(str8* str, u64 cap) {
    str->content = (char*) calloc(cap, 1);

    if (NULL == str->content) {
        return -1;
    }

    str->capacity = cap;
    str->length = 0;

    return 0;
}

i8 str8_attach_nt(str8* str, const char* strnt) {
    if (NULL == strnt) {
        return -1;
    }

    str->content = (char*) strnt;
    str->capacity = strlen((char*) strnt);
    str->length = str->capacity;

    return 0;
}

void str8_free(str8* str) {
    free(str->content);

    str->content = NULL;
    str->capacity = 0;
    str->length = 0;
}

const char* str8_dattach_nt(str8* str) {
    const char* content = (const char*) str->content;

    str->content = NULL;
    str->capacity = 0;
    str->length = 0;

    return content;
}
