#include <ctype.h>
#include <string.h>

#include "mctypes.h"

#include "mclstr.h"
#include "mcheap.h"

heap_header* mclstr_alloc(u32 cap, MCHeap* heap) {
    heap_header* hdr = mcheap_managed_reserve(heap, sizeof(mclstr) + cap);
    if (NULL == hdr) {
        return NULL;
    }

    hdr->type = OBJ_STR;
    hdr->object.string->len = 0;

    return hdr;
}

i8 mclstr_append_lstr(heap_header* dst, heap_header* src, MCHeap* heap) {
    i8 rcode = 0;
    u32 oldlen = dst->object.string->len;

    if ((sizeof(mclstr) + dst->object.string->len + src->object.string->len) > dst->objcap) {
        rcode = mcheap_managed_resize(heap, dst,
                sizeof(mclstr) + dst->object.string->len + src->object.string->len);
        if (0 != rcode) {
            return -1;
        }
    }

    dst->object.string->len += src->object.string->len;
    memcpy(mclstr_getchars(dst) + oldlen,
            mclstr_getchars(src),
            src->object.string->len);

    return 0;
}

i8 mclstr_append_str0(heap_header* dst,
        const char* str0, u32 str0len, MCHeap* heap) {
    i8 rcode = 0;
    u32 oldlen = dst->object.string->len;

    if ((sizeof(mclstr) + dst->object.string->len + str0len) > dst->objcap) {
        rcode = mcheap_managed_resize(heap, dst,
                sizeof(mclstr) + dst->object.string->len + str0len);
        if (0 != rcode) {
            return -1;
        }
    }

    dst->object.string->len += str0len;
    memcpy(mclstr_getchars(dst) + oldlen, str0, str0len);

    return 0;
}

char* mclstr_getchars(heap_header* str) {
    return (char*) str->object.string + sizeof(mclstr);
}

u8 mclstr_byte(heap_header* str, u32 idx) {
    if (idx >= str->object.string->len) {
        return 0;
    }
    return (u8) mclstr_getchars(str)[idx];
}

i8 mclstr_cmp(heap_header* str1, heap_header* str2) {
    u32 minlen = 0;
    i8 result = 0;

    if (str1->object.string->len < str2->object.string->len) {
        minlen = str1->object.string->len;
    } else {
        minlen = str2->object.string->len;
    }

    result = memcmp(mclstr_getchars(str1), mclstr_getchars(str2), minlen);

    if (0 != result) {
        return result;
    }

    if (str1->object.string->len < str2->object.string->len) {
        return -1;
    };

    if (str1->object.string->len > str2->object.string->len) {
        return 1;
    };

    return 0;
}

i8 mclstr_ptrcmp(heap_header* str1, heap_header* str2) {
    return (str1->object.string == str2->object.string);
}

void mclstr_upper(heap_header* str) {
    char* chars = mclstr_getchars(str);

    for (u32 idx = 0; idx < str->object.string->len; idx += 1) {
        chars[idx] = (char) toupper((uchar) chars[idx]);
    }
}

void mclstr_lower(heap_header* str) {
    char* chars = mclstr_getchars(str);

    for (u32 idx = 0; idx < str->object.string->len; idx += 1) {
        chars[idx] = (char) tolower((uchar) chars[idx]);
    }
}

void mclstr_reverse(heap_header* str) {
    char* chars = mclstr_getchars(str);
    u32 low = 0;
    u32 high = 0;
    char tmp = 0;

    if (0 == str->object.string->len) {
        return;
    }

    high = str->object.string->len - 1;
    while (low < high) {
        tmp = chars[low];
        chars[low] = chars[high];
        chars[high] = tmp;
        low += 1;
        high -= 1;
    }
}

void mclstr_clear(heap_header* str) {
    str->object.string->len = 0;
}

heap_header* mclstr_sub(u32 start, u32 end, heap_header* str, MCHeap* heap) {
    heap_header* result = NULL;
    u32 sublen = 0;

    if ((start > end) || (end > str->object.string->len)) {
        return NULL;
    }

    sublen = (end - start);
    result = mclstr_alloc(sublen, heap);
    if (NULL == result) {
        return NULL;
    }

    memcpy(mclstr_getchars(result), mclstr_getchars(str) + start, sublen);
    result->object.string->len = sublen;

    return result;
}

heap_header* mclstr_rep(u32 n, heap_header* str, MCHeap* heap) {
    heap_header* result = NULL;
    u32 srclen = 0;
    char* dst = NULL;

    if ((0 == n) || (0 == str->object.string->len)) {
        return NULL;
    }

    srclen = str->object.string->len;
    result = mclstr_alloc((srclen * n), heap);
    if (NULL == result) {
        return NULL;
    }

    dst = mclstr_getchars(result);
    for (u32 idx = 0; idx < n; idx += 1) {
        memcpy(dst + (idx * srclen), mclstr_getchars(str), srclen);
    }
    result->object.string->len = (srclen * n);

    return result;
}
